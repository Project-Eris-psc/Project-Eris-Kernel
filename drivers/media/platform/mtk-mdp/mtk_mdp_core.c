/*
 * Copyright (c) 2015-2016 MediaTek Inc.
 * Author: Houlong Wei <houlong.wei@mediatek.com>
 *         Ming Hsiu Tsai <minghsiu.tsai@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/workqueue.h>
#include <soc/mediatek/smi.h>
#include <linux/iommu.h>

#include "mtk_mdp_core.h"
#include "mtk_mdp_m2m.h"
#ifdef CONFIG_VIDEO_MEDIATEK_VCU
#include "mtk_mdp_vpu.h"
#else
#include "mtk_vpu.h"
#endif

/* MDP debug log level (0-3). 3 shows all the logs. */
int mtk_mdp_dbg_level;
EXPORT_SYMBOL(mtk_mdp_dbg_level);

module_param(mtk_mdp_dbg_level, int, 0644);

static const struct of_device_id mtk_mdp_comp_dt_ids[] = {
	{
		.compatible = "mediatek,mt8173-mdp-rdma",
		.data = (void *)MTK_MDP_RDMA
	}, {
		.compatible = "mediatek,mt8173-mdp-rsz",
		.data = (void *)MTK_MDP_RSZ
	}, {
		.compatible = "mediatek,mt8173-mdp-wdma",
		.data = (void *)MTK_MDP_WDMA
	}, {
		.compatible = "mediatek,mt8173-mdp-wrot",
		.data = (void *)MTK_MDP_WROT
	}, {
		.compatible = "mediatek,mt2712-mdp-rdma",
		.data = (void *)MTK_MDP_RDMA
	}, {
		.compatible = "mediatek,mt2712-mdp-rsz",
		.data = (void *)MTK_MDP_RSZ
	}, {
		.compatible = "mediatek,mt2712-mdp-tdshp",
		.data = (void *)MTK_MDP_TDSHP
	}, {
		.compatible = "mediatek,mt2712-mdp-wdma",
		.data = (void *)MTK_MDP_WDMA
	}, {
		.compatible = "mediatek,mt2712-mdp-wrot",
		.data = (void *)MTK_MDP_WROT
	}, {
		.compatible = "mediatek,mt8167-mdp-rdma",
		.data = (void *)MTK_MDP_RDMA
	}, {
		.compatible = "mediatek,mt8167-mdp-rsz",
		.data = (void *)MTK_MDP_RSZ
	}, {
		.compatible = "mediatek,mt8167-mdp-tdshp",
		.data = (void *)MTK_MDP_TDSHP
	}, {
		.compatible = "mediatek,mt8167-mdp-wdma",
		.data = (void *)MTK_MDP_WDMA
	}, {
		.compatible = "mediatek,mt8167-mdp-wrot",
		.data = (void *)MTK_MDP_WROT
	},
	{ },
};

static const struct of_device_id mtk_mdp_of_ids[] = {
	{ .compatible = "mediatek,mt2701-mdp", .data = "platform:mt2701" },
	{ .compatible = "mediatek,mt8173-mdp", .data = "platform:mt8173" },
	{ .compatible = "mediatek,mt2712-mdp", .data = "platform:mt2712" },
	{ .compatible = "mediatek,mt8167-mdp", .data = "platform:mt8167" },
	{ },
};
MODULE_DEVICE_TABLE(of, mtk_mdp_of_ids);

static void mtk_mdp_clock_on(struct mtk_mdp_dev *mdp)
{
	struct device *dev = &mdp->pdev->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(mdp->comp); i++)
		if (mdp->comp[i] != NULL)
			mtk_mdp_comp_clock_on(dev, mdp->comp[i]);
}

static void mtk_mdp_clock_off(struct mtk_mdp_dev *mdp)
{
	struct device *dev = &mdp->pdev->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(mdp->comp); i++)
		if (mdp->comp[i] != NULL)
			mtk_mdp_comp_clock_off(dev, mdp->comp[i]);
}

static void mtk_mdp_wdt_worker(struct work_struct *work)
{
	struct mtk_mdp_dev *mdp =
			container_of(work, struct mtk_mdp_dev, wdt_work);
	struct mtk_mdp_ctx *ctx;

	mtk_mdp_err("Watchdog timeout");

	list_for_each_entry(ctx, &mdp->ctx_list, list)
		mtk_mdp_ctx_state_lock_set(ctx, MTK_MDP_CTX_ERROR);
}

#ifndef CONFIG_VIDEO_MEDIATEK_VCU
static void mtk_mdp_reset_handler(void *priv)
{
	struct mtk_mdp_dev *mdp = priv;

	queue_work(mdp->wdt_wq, &mdp->wdt_work);
}
#endif

static int mtk_mdp_probe(struct platform_device *pdev)
{
	struct mtk_mdp_dev *mdp;
	struct device *dev = &pdev->dev;
	struct device_node *node;
	struct platform_device *cmdq_dev;
	int i, ret = 0;
	struct iommu_domain *iommu;

	iommu = iommu_get_domain_for_dev(dev);
	if (!iommu) {
		dev_info(dev, "Waiting iommu driver ready...\n");
		return -EPROBE_DEFER;
	}
	/* Check whether cmdq driver is ready */
	node = of_parse_phandle(dev->of_node, "mediatek,gce", 0);
	if (!node) {
		dev_err(dev, "cannot get gce node handle\n");
		return -EINVAL;
	}

	cmdq_dev = of_find_device_by_node(node);
	if (!cmdq_dev || !cmdq_dev->dev.driver) {
		dev_err(dev, "Waiting cmdq driver ready...\n");
		of_node_put(node);
		return -EPROBE_DEFER;
	}

	mdp = devm_kzalloc(dev, sizeof(*mdp), GFP_KERNEL);
	if (!mdp)
		return -ENOMEM;

	ret = of_property_read_u32(dev->of_node, "mediatek,mdpid",
		(u32 *)(void *)&mdp->id);
	if (ret) {
		dev_info(dev, "not set mediatek,mdpid, use default id 0.\n");
		mdp->id = 0;
	}

	if (mdp->id == 0)
		strlcpy(mdp->driver, MTK_MDP_MODULE_NAME, sizeof(mdp->driver));
	else
		ret = snprintf(mdp->driver, sizeof(mdp->driver), "%s-%d",
			MTK_MDP_MODULE_NAME, mdp->id);

	strlcpy(mdp->platform, of_device_get_match_data(dev),
		sizeof(mdp->platform));
	mdp->pdev = pdev;
	INIT_LIST_HEAD(&mdp->ctx_list);

	mutex_init(&mdp->lock);
	mutex_init(&mdp->vpulock);

	/* Iterate over sibling MDP function blocks */
	for_each_child_of_node(dev->of_node->parent, node) {
		const struct of_device_id *of_id;
		enum mtk_mdp_comp_type comp_type;
		int comp_id;
		struct mtk_mdp_comp *comp;

		of_id = of_match_node(mtk_mdp_comp_dt_ids, node);
		if (!of_id)
			continue;

		if (!of_device_is_available(node)) {
			dev_err(dev, "Skipping disabled component %s\n",
				node->full_name);
			continue;
		}

		comp_type = (enum mtk_mdp_comp_type)of_id->data;
		comp_id = mtk_mdp_comp_get_id(dev, node, comp_type);
		if (comp_id < 0) {
			dev_warn(dev, "Skipping unknown component %s\n",
				 node->full_name);
			continue;
		}

		comp = devm_kzalloc(dev, sizeof(*comp), GFP_KERNEL);
		if (!comp) {
			ret = -ENOMEM;
			goto err_comp;
		}
		mdp->comp[comp_id] = comp;

		ret = mtk_mdp_comp_init(dev, node, comp, comp_id);
		if (ret)
			goto err_comp;
	}

	mdp->job_wq = create_singlethread_workqueue(MTK_MDP_MODULE_NAME);
	if (!mdp->job_wq) {
		dev_err(dev, "unable to alloc job workqueue\n");
		ret = -ENOMEM;
		goto err_alloc_job_wq;
	}

	mdp->wdt_wq = create_singlethread_workqueue("mdp_wdt_wq");
	if (!mdp->wdt_wq) {
		dev_err(dev, "unable to alloc wdt workqueue\n");
		ret = -ENOMEM;
		goto err_alloc_wdt_wq;
	}

	INIT_WORK(&mdp->wdt_work, mtk_mdp_wdt_worker);

	ret = v4l2_device_register(dev, &mdp->v4l2_dev);
	if (ret) {
		dev_err(dev, "Failed to register v4l2 device\n");
		ret = -EINVAL;
		goto err_dev_register;
	}

	ret = mtk_mdp_register_m2m_device(mdp);
	if (ret) {
		v4l2_err(&mdp->v4l2_dev, "Failed to init mem2mem device\n");
		goto err_m2m_register;
	}

	mdp->vpu_dev = vpu_get_plat_device(pdev);
#ifndef CONFIG_VIDEO_MEDIATEK_VCU
	vpu_wdt_reg_handler(mdp->vpu_dev, mtk_mdp_reset_handler, mdp,
			    VPU_RST_MDP);
#endif

	platform_set_drvdata(pdev, mdp);

	mdp->alloc_ctx = vb2_dma_contig_init_ctx(dev);
	if (IS_ERR(mdp->alloc_ctx)) {
		ret = PTR_ERR(mdp->alloc_ctx);
		goto err_alloc_ctx;
	}

	if (!dev->dma_parms) {
		dev->dma_parms = kzalloc(sizeof(*dev->dma_parms), GFP_KERNEL);
		if (!dev->dma_parms) {
			dev_err(dev, "Failed to alloc for dma_parms\n");
			goto err_alloc_dma_parms;
		}
	}
	if (dma_set_max_seg_size(dev, DMA_BIT_MASK(32)) != 0) {
		dev_err(dev, "Failed to dma_set_max_seg_size\n");
		goto err_set_dma_seg_size;
	}

	pm_runtime_enable(dev);

	mdp->cmdq_client = cmdq_mbox_create(dev, 0);

	dev_dbg(dev, "mdp-%d registered successfully\n", mdp->id);

	return 0;

err_set_dma_seg_size:
	kfree(dev->dma_parms);

err_alloc_dma_parms:

err_alloc_ctx:
	mtk_mdp_unregister_m2m_device(mdp);

err_m2m_register:
	v4l2_device_unregister(&mdp->v4l2_dev);

err_dev_register:
	destroy_workqueue(mdp->wdt_wq);

err_alloc_wdt_wq:
	destroy_workqueue(mdp->job_wq);

err_alloc_job_wq:

err_comp:
	for (i = 0; i < ARRAY_SIZE(mdp->comp); i++)
		mtk_mdp_comp_deinit(dev, mdp->comp[i]);

	dev_dbg(dev, "err %d\n", ret);
	return ret;
}

static int mtk_mdp_remove(struct platform_device *pdev)
{
	struct mtk_mdp_dev *mdp = platform_get_drvdata(pdev);
	int i;

	pm_runtime_disable(&pdev->dev);
	vb2_dma_contig_cleanup_ctx(mdp->alloc_ctx);
	mtk_mdp_unregister_m2m_device(mdp);
	v4l2_device_unregister(&mdp->v4l2_dev);

	flush_workqueue(mdp->job_wq);
	destroy_workqueue(mdp->job_wq);

	for (i = 0; i < ARRAY_SIZE(mdp->comp); i++)
		mtk_mdp_comp_deinit(&pdev->dev, mdp->comp[i]);

	cmdq_mbox_destroy(mdp->cmdq_client);

	kfree(pdev->dev.dma_parms);

	dev_dbg(&pdev->dev, "%s driver unloaded\n", pdev->name);
	return 0;
}

static int __maybe_unused mtk_mdp_pm_suspend(struct device *dev)
{
	struct mtk_mdp_dev *mdp = dev_get_drvdata(dev);

	mtk_mdp_clock_off(mdp);

	return 0;
}

static int __maybe_unused mtk_mdp_pm_resume(struct device *dev)
{
	struct mtk_mdp_dev *mdp = dev_get_drvdata(dev);

	mtk_mdp_clock_on(mdp);

	return 0;
}

static int __maybe_unused mtk_mdp_suspend(struct device *dev)
{
	if (pm_runtime_suspended(dev))
		return 0;

	return mtk_mdp_pm_suspend(dev);
}

static int __maybe_unused mtk_mdp_resume(struct device *dev)
{
	if (pm_runtime_suspended(dev))
		return 0;

	return mtk_mdp_pm_resume(dev);
}

static const struct dev_pm_ops mtk_mdp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_mdp_suspend, mtk_mdp_resume)
	SET_RUNTIME_PM_OPS(mtk_mdp_pm_suspend, mtk_mdp_pm_resume, NULL)
};

static struct platform_driver mtk_mdp_driver = {
	.probe		= mtk_mdp_probe,
	.remove		= mtk_mdp_remove,
	.driver = {
		.name	= MTK_MDP_MODULE_NAME,
		.pm	= &mtk_mdp_pm_ops,
		.of_match_table = mtk_mdp_of_ids,
	}
};

module_platform_driver(mtk_mdp_driver);

MODULE_AUTHOR("Houlong Wei <houlong.wei@mediatek.com>");
MODULE_DESCRIPTION("Mediatek image processor driver");
MODULE_LICENSE("GPL v2");
