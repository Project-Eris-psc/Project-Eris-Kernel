#! /usr/bin/python
# -*- coding: utf-8 -*-

import re
import os
import sys
import string
import ConfigParser
import xml.dom.minidom


from data.GpioData import GpioData
from data.EintData import EintData
from ModuleObj import ModuleObj
import ChipObj
from utility.util import compare
from utility.util import sorted_key
from utility.util import log
from utility.util import LogLevel

class GpioObj(ModuleObj):
    def __init__(self):
        ModuleObj.__init__(self,'cust_gpio_boot.h', 'cust_gpio.dtsi')
        self.__fileName = 'cust_gpio_usage.h'
        self.__filePinfunc = '%s-pinfunc.h' %(ModuleObj.get_chipId().lower())
        self.__filePinCtrl = 'pinctrl-mtk-%s.h' %(ModuleObj.get_chipId().lower())
        self.__fileScp = 'cust_scp_gpio_usage.h'
        self.__fileMap = 'cust_gpio_usage_mapping.dtsi'

    def get_cfgInfo(self):
        cp = ConfigParser.ConfigParser(allow_no_value=True)
        cp.read(ModuleObj.get_cmpPath())

        # get GPIO_FREQ section
        keys= cp.options('GPIO_FREQ')
        for key in keys:
            value = cp.get('GPIO_FREQ', key)
            GpioData._freqMap[key] = value

        # get GPIO_MODE section
        keys = cp.options('GPIO_MODE')
        for key in keys:
            value = cp.get('GPIO_MODE', key)
            GpioData._specMap[key] = value

        GpioData._mapList = cp.options('GPIO_VARIABLES_MAPPING')

        cp.read(ModuleObj.get_figPath())
        ops = cp.options('GPIO')
        for op in ops:
            value = cp.get('GPIO', op)
            list = re.split(r' +|\t+', value)
            tmp_list = list[0:len(list)-2]
            temp = []
            for item in tmp_list:
                str = item[6:len(item)-1]
                temp.append(str)
            GpioData._modeMap[op] = temp

            data = GpioData()
            data.set_smtNum(string.atoi(list[len(list)-1]))
            ModuleObj.set_data(self, op.lower(), data)

    def read(self, node):
        nodes = node.childNodes
        for node in nodes:
            if node.nodeType == xml.dom.Node.ELEMENT_NODE:
                if cmp(node.nodeName, 'count') == 0:
                    GpioData._count = string.atoi(node.childNodes[0].nodeValue)
                    continue

                eintNode = node.getElementsByTagName('eint_mode')
                defmNode = node.getElementsByTagName('def_mode')
                modsNode = node.getElementsByTagName('mode_arr')
                inpeNode = node.getElementsByTagName('inpull_en')
                inpsNode = node.getElementsByTagName('inpull_selhigh')
                defdNode = node.getElementsByTagName('def_dir')
                diriNode = node.getElementsByTagName('in')
                diroNode = node.getElementsByTagName('out')
                outhNode = node.getElementsByTagName('out_high')
                var0Node = node.getElementsByTagName('varName0')
                var1Node = node.getElementsByTagName('varName1')
                var2Node = node.getElementsByTagName('varName2')
                smtNode = node.getElementsByTagName('smt')
                iesNode = node.getElementsByTagName('ies')

                num = string.atoi(node.nodeName[4:])
                if num >= len(ModuleObj.get_data(self)):
                    break
                data = ModuleObj.get_data(self)[node.nodeName]

                if len(eintNode):
                    flag = False
                    if cmp(eintNode[0].childNodes[0].nodeValue, 'true') == 0:
                        flag = True
                    data.set_eintMode(flag)

                if len(defmNode):
                    data.set_defMode(string.atoi(defmNode[0].childNodes[0].nodeValue))

                if len(modsNode):
                    str = modsNode[0].childNodes[0].nodeValue
                    temp_list = []
                    for i in range(0, len(str)):
                        temp_list.append(str[i])
                    data.set_modeVec(temp_list)

                if len(inpeNode):
                    flag = False
                    if cmp(inpeNode[0].childNodes[0].nodeValue, 'true') == 0:
                        flag = True
                    data.set_inpullEn(flag)

                if len(inpsNode):
                    flag = False
                    if cmp(inpsNode[0].childNodes[0].nodeValue, 'true') == 0:
                        flag = True
                    data.set_inpullSelHigh(flag)

                if len(defdNode):
                    data.set_defDir(defdNode[0].childNodes[0].nodeValue)

                if len(diriNode):
                    flag = False
                    if cmp(diriNode[0].childNodes[0].nodeValue, 'true') == 0:
                        flag = True
                    data.set_inEn(flag)

                if len(diroNode):
                    flag = False
                    if cmp(diroNode[0].childNodes[0].nodeValue, 'true') == 0:
                        flag = True
                    data.set_outEn(flag)

                if len(outhNode):
                    flag = False
                    if cmp(outhNode[0].childNodes[0].nodeValue, 'true') == 0:
                        flag = True
                    data.set_outHigh(flag)

                temp_list= []

                if len(var0Node) != 0 and len(var0Node[0].childNodes) != 0:
                    temp_list.append(var0Node[0].childNodes[0].nodeValue)
                if len(var1Node) != 0 and len(var1Node[0].childNodes) != 0:
                    temp_list.append(var1Node[0].childNodes[0].nodeValue)
                if len(var2Node) != 0 and len(var2Node[0].childNodes) != 0:
                    temp_list.append(var2Node[0].childNodes[0].nodeValue)
                data.set_varNames(temp_list)

                if len(smtNode):
                    flag = False
                    if cmp(smtNode[0].childNodes[0].nodeValue, 'true') == 0:
                        flag = True
                    data.set_smtEn(flag)

                if len(iesNode):
                    flag = False
                    if cmp(iesNode[0].childNodes[0].nodeValue, 'true') == 0:
                        flag = True
                    data.set_iesEn(flag)

                ModuleObj.set_data(self, node.nodeName, data)

        return True

    def get_gpioData(self, idx):
        if idx >= GpioData._count or idx < 0:
            return None

        key = 'gpio%s' %(idx)
        return ModuleObj.get_data(self)[key]

    def parse(self, node):
        self.get_cfgInfo()
        self.read(node)

    def isMuxMode(self, key, index, modIdx):
        mode_name = GpioData.get_modeName(key, index)
        modIdx.append(index)

        if mode_name.find('//') != -1:
            return True
        return False

    def gen_files(self):
        ModuleObj.gen_files(self)
        self.gen_cFile()
        self.gen_specFiles()

    def gen_spec(self, para):
        if para == 'gpio_usage_h':
            self.gen_cFile()
        elif para == 'gpio_boot_h':
            self.gen_hFile()
        elif para == 'gpio_dtsi':
            self.gen_dtsiFile()
        elif para == 'scp_gpio_usage_h':
            self.gen_scpUsage()
        elif para == 'pinctrl_h':
            self.gen_pinCtrl()
        elif para == 'pinfunc_h':
            self.gen_pinFunc()
        elif para == 'gpio_usage_mapping_dtsi':
            self.gen_mapDtsi()


    def gen_cFile(self):
        gen_str = ''
        fp = open(os.path.join(ModuleObj.get_genPath(), self.__fileName), 'w')
        gen_str += ModuleObj.writeComment()
        gen_str += ModuleObj.writeHeader(self.__fileName)
        gen_str += self.fill_cFile()
        gen_str += ModuleObj.writeTail(self.__fileName)
        fp.write(gen_str)
        fp.close()

    def gen_specFiles(self):
        self.gen_pinFunc()
        self.gen_pinCtrl()
        self.gen_scpUsage()
        self.gen_mapDtsi()

    def gen_pinFunc(self):
        gen_str = ''
        fp = open(os.path.join(ModuleObj.get_genPath(), self.__filePinfunc), 'w')
        gen_str += ModuleObj.writeComment()
        gen_str += ModuleObj.writeHeader(self.__filePinfunc)
        gen_str += self.fill_pinfunc_hFile()
        gen_str += ModuleObj.writeTail(self.__filePinfunc)
        fp.write(gen_str)
        fp.close()

    def gen_pinCtrl(self):
        gen_str = ''
        fp = open(os.path.join(ModuleObj.get_genPath(), self.__filePinCtrl), 'w')
        gen_str += ModuleObj.writeComment()
        gen_str += ModuleObj.writeHeader(self.__filePinCtrl)
        gen_str += self.fill_pinctrl_hFile()
        gen_str += ModuleObj.writeTail(self.__filePinCtrl)
        fp.write(gen_str)
        fp.close()

    def gen_scpUsage(self):
        gen_str = ''
        fp = open(os.path.join(ModuleObj.get_genPath(), self.__fileScp), 'w')
        gen_str += ModuleObj.writeComment()
        gen_str += ModuleObj.writeHeader(self.__fileScp)
        gen_str += self.fill_cFile()
        gen_str += ModuleObj.writeTail(self.__fileScp)
        fp.write(gen_str)
        fp.close()

    def gen_mapDtsi(self):
        gen_str = ''
        fp = open(os.path.join(ModuleObj.get_genPath(), self.__fileMap), 'w')
        gen_str += ModuleObj.writeComment()
        gen_str += self.fill_mapping_dtsiFile()
        fp.write(gen_str)
        fp.close()

    def fill_hFile(self):
        gen_str = '''//Configuration for GPIO SMT(Schmidt Trigger) Group output start\n'''
        temp_list = []
        for key in sorted_key(ModuleObj.get_data(self).keys()):
        #for value in ModuleObj.get_data(self).values():
            value = ModuleObj.get_data(self)[key]
            num = value.get_smtNum()
            if num in temp_list or num < 0:
                continue
            else:
                temp_list.append(num)
                if value.get_smtEn():
                    gen_str += '''#define GPIO_SMT_GROUP_%d\t\t1\n''' %(num)
                else:
                    gen_str += '''#define GPIO_SMT_GROUP_%d\t\t0\n''' %(num)

        gen_str += '''\n\n'''

        sorted_list = sorted(ModuleObj.get_data(self).keys(), key = compare)

        for key in sorted_list:
            value = ModuleObj.get_data(self)[key]
            if self.is_i2cPadPin(value.get_modeName(key, value.get_defMode())):
                value.set_inpullEn(False)
                value.set_outHigh(False)
                value.set_inpullSelHigh(False)

            gen_str += '''//Configuration for %s\n''' %(key.upper())

            mode_name = GpioData.get_modeName(key, value.get_defMode())
            val = ''
            if mode_name != '':
                flag = False
                #val = ''
                if mode_name.find('//') != -1:
                    flag = True

                value.get_modeVec()[value.get_defMode()]
                if flag:
                    if value.get_modeVec()[value.get_defMode()] == '1':
                        val = str(value.get_defMode())
                    elif value.get_modeVec()[value.get_defMode()] == '2':
                        val = str(value.get_defMode() + GpioData._modNum)
                else:
                    val = str(value.get_defMode())

            if len(val) < 2:
                val = '0' + val

            pull_en = ''
            if value.get_inPullEn():
                pull_en = 'ENABLE'
            else:
                pull_en = 'DISABLE'

            pull_sel = ''
            if value.get_inPullSelHigh():
                pull_sel = 'UP'
            else:
                pull_sel = 'DOWN'

            out_high = ''
            if value.get_outHigh():
                out_high = 'ONE'
            else:
                out_high = 'ZERO'

            smt_en = ''
            if value.get_smtEn():
                smt_en = 'ENABLE'
            else:
                smt_en= 'DISABLE'

            ies_en = ''
            if value.get_iesEn():
                ies_en = 'ENABLE'
            else:
                ies_en = 'DISABLE'

            gen_str += '''#define %s_MODE\t\t\tGPIO_MODE_%s\n''' %(key.upper(), val)
            gen_str += '''#define %s_DIR\t\t\tGPIO_DIR_%s\n''' %(key.upper(), value.get_defDir())
            gen_str += '''#define %s_PULLEN\t\tGPIO_PULL_%s\n''' %(key.upper(), pull_en)
            gen_str += '''#define %s_PULL\t\t\tGPIO_PULL_%s\n''' %(key.upper(), pull_sel)
            gen_str += '''#define %s_DATAOUT\t\tGPIO_OUT_%s\n''' %(key.upper(), out_high)
            gen_str += '''#define %s_SMT\t\t\tGPIO_SMT_%s\n''' %(key.upper(), smt_en)
            gen_str += '''#define %s_IES\t\t\tGPIO_IES_%s\n\n''' %(key.upper(), ies_en)

        return gen_str


    def is_i2cPadPin(self, name):
        if re.match(r'^SCL\d+$', name) or re.match(r'^SDA\d+$', name):
            return True

        return False

    def fill_cFile(self):
        gen_str = ''
        #sorted_list = sorted(ModuleObj.get_data(self).keys(), key = compare)

        for key in sorted_key(ModuleObj.get_data(self).keys()):
        #for key in sorted_list:
            value = ModuleObj.get_data(self)[key]
            for varName in value.get_varNames():
                gen_str += '''#define %s\t\t\t(%s | 0x80000000)\n''' %(varName.upper(), key.upper())

                if value.get_eintMode():
                    gen_str += '''#define %s_M_EINT\t\tGPIO_MODE_00\n''' %(varName)

                temp_list = []
                for item in GpioData._specMap.keys():
                    regExp = '[_A-Z0-9:]*%s[_A-Z0-9:]*' %(item.upper())
                    pat = re.compile(regExp)
                    for i in range(0, GpioData._modNum):
                        list = value.get_modeVec()
                        mode_name = GpioData.get_modeName(key, i)

                        if list[i] == '1':
                            if mode_name.find('//') != -1:
                                mode_name = mode_name.split('//')[0]
                        elif list[i] == '2':
                            if mode_name.find('//') != -1:
                                mode_name = mode_name.split('//')[1]

                        if pat.match(mode_name):
                            if cmp(item, 'eint') == 0 and ((value.get_eintMode() or mode_name.find('MD_EINT') != -1)):
                                continue

                            gen_str += '''#define %s%s\t\tGPIO_MODE_0%d\n''' %(varName.upper(), GpioData._specMap[item].upper(), i)
                            temp_list.append(i)
                            break

                if not value.get_eintMode():
                    list = value.get_modeVec()
                    for i in range(0,GpioData._modNum):
                        mode_name = GpioData.get_modeName(key, i)

                        if list[i] == '0':
                            continue
                        elif list[i] == '1':
                            if mode_name.find('//') != -1:
                                mode_name = mode_name.split('//')[0]
                        elif list[i] == '2':
                            if mode_name.find('//') != -1:
                                mode_name = mode_name.split('//')[1]


                        if not i in temp_list:
                            gen_str += '''#define %s_M_%s\t\tGPIO_MODE_0%d\n''' %(varName, re.sub(r'\d{0,3}$', '', mode_name), i)

                regExp = r'CLKM\d'
                pat = re.compile(regExp)
                for i in range(0, GpioData._modNum):
                    mode = GpioData.get_modeName(key, i)
                    if pat.match(mode):
                        gen_str += '''#define %s_CLK\t\tCLK_OUT%s\n''' %(varName, mode[4:])
                        temp = ''
                        if varName in GpioData._freqMap.keys():
                            temp = GpioData._freqMap[varName]
                        else:
                            temp = 'GPIO_CLKSRC_NONE'
                        gen_str += '''#define %s_FREQ\t\t%s\n''' %(varName, temp)

                gen_str += '''\n'''

        return gen_str


    def fill_dtsiFile(self):
        gen_str = '''&pio {\n\n'''
        gen_str += '''\tgpio_pins_default: gpiodef{\n\t};\n\n'''

        #sorted_list = sorted(ModuleObj.get_data(self).keys(), key = compare)
        for key in sorted_key(ModuleObj.get_data(self).keys()):
        #for key in sorted_list:
            value = ModuleObj.get_data(self)[key]
            gen_str += '''\t%s: gpio@%s {\n''' %(key.lower(), key[4:])
            gen_str += '''\t\tpins_cmd_dat {\n'''
            mode = value.get_defMode()
            mode_name = GpioData.get_modeName(key, mode)
            mode_val = value.get_modeVec()[mode]
            if mode_val == '1':
                if mode_name.find('//') != -1:
                    mode_name = mode_name.split('//')[0]
            elif mode_val == '2':
                if mode_name.find('//') != -1:
                    mode_name = mode_name.split('//')[1]

            gen_str += '''\t\t\tpins = <PINMUX_GPIO%s__FUNC_%s>;\n''' %(key[4:], mode_name)
            gen_str += '''\t\t\tslew-rate = <%d>;\n''' %(value.ge_defDirInt())

            temp = ''
            if not value.get_inPullEn():
                temp = 'bias-disable;'
            gen_str += '''\t\t\t%s\n''' %(temp)
            if value.get_inPullSelHigh():
                temp = '11'
            else:
                temp = '00'
            gen_str += '''\t\t\tbias-pull-down = <%s>;\n''' %(temp)
            if value.get_outHigh():
                temp = 'high'
            else:
                temp = 'low'
            gen_str += '''\t\t\toutput-%s;\n''' %(temp)
            gen_str += '''\t\t\tinput-schmitt-enable = <%d>;\n''' %(value.get_smtEn())
            gen_str += '''\t\t};\n'''
            gen_str += '''\t};\n'''

        gen_str += '''};\n\n'''

        gen_str += '''&gpio {\n'''
        lineLen = 0
        gen_str += '''\tpinctrl-names = "default",'''
        lineLen += 30
        for i in range(0, GpioData._count-1):
            gen_str += '''"gpio%d",''' %(i)
            if i < 10:
                lineLen += 8
            elif i < 100:
                lineLen += 9
            elif i >= 100:
                lineLen += 10

            if lineLen > 100:
                gen_str += '''\n'''
                lineLen = 0


        gen_str += '''"gpio%d";\n''' %(GpioData._count-1)
        gen_str += '''\tpinctrl-0 = <&gpio_pins_default>;\n'''

        for i in range(1, GpioData._count):
            gen_str += '''\tpinctrl-%d = <&gpio%d>;\n''' %(i, i-1)

        gen_str += '''\n'''
        gen_str += '''\tstatus = \"okay\";\n'''
        gen_str += '''};\n'''

        return gen_str

    def fill_pinfunc_hFile(self):
        gen_str = '''#include \"mt65xx.h\"\n\n'''
        #sorted_list = sorted(ModuleObj.get_data(self).keys(), key = compare)
        for key in sorted_key(ModuleObj.get_data(self).keys()):
        #for key in sorted_list:
            value = ModuleObj.get_data(self)[key]
            for i in range(0, GpioData._modNum):
                mode_name = GpioData.get_modeName(key, i)

                if mode_name != '':
                    lst = []
                    if mode_name.find('//') != -1:
                        lst = mode_name.split('//')
                    else:
                        lst.append(mode_name)

                    for j in range(0, len(lst)):
                        gen_str += '''#define PINMUX_GPIO%s__FUNC_%s (MTK_PIN_NO(%s) | %d)\n''' %(key[4:], lst[j], key[4:], (i + j*8))

            gen_str += '''\n'''
        gen_str += '''\n'''

        return gen_str

    def fill_pinctrl_hFile(self):
        gen_str = '''#include <linux/pinctrl/pinctrl.h>\n'''
        gen_str += '''#include <pinctrl-mtk-common.h>\n\n'''
        gen_str += '''static const struct mtk_desc_pin mtk_pins_%s[] = {\n''' %(ModuleObj.get_chipId().lower())

        #sorted_list = sorted(ModuleObj.get_data(self).keys(), key = compare)
        for key in sorted_key(ModuleObj.get_data(self).keys()):
        #for key in sorted_list:
            value = ModuleObj.get_data(self)[key]
            gen_str += '''\tMTK_PIN(\n'''
            gen_str += '''\t\tPINCTRL_PIN(%s, \"%s\"),\n''' %(key[4:], key.upper())
            gen_str += '''\t\tNULL, \"%s\",\n''' %(ModuleObj.get_chipId().lower())
            gen_str += '''\t\tMTK_EINT_FUNCTION(NO_EINT_SUPPORT, NO_EINT_SUPPORT)'''
            for i in range(0, GpioData._modNum):
                mode_name = GpioData.get_modeName(key, i)

                if mode_name != '':
                    lst = []
                    if mode_name.find('//') != -1:
                        lst = mode_name.split('//')
                    else:
                        lst.append(mode_name)
                    for j in range(0, len(lst)):
                        gen_str += ''',\n\t\tMTK_FUNCTION(%d, "%s")''' %(i + j * 8, lst[j])
            gen_str += '''\n\t),\n'''

        gen_str += '''};\n'''

        return gen_str

    def fill_mapping_dtsiFile(self):
        gen_str = '''&gpio_usage_mapping {\n'''

        #sorted_list = sorted(ModuleObj.get_data(self).keys(), key = compare)
        for key in sorted_key(ModuleObj.get_data(self).keys()):
        #for key in sorted_list:
            value = ModuleObj.get_data(self)[key]
            for varName in value.get_varNames():
                if varName != '' and varName.lower() in GpioData._mapList:
                    gen_str += '''\t%s = <%s>;\n''' %(varName, key[4:])

        gen_str += '''};\n'''
        return gen_str

class GpioObj_whitney(GpioObj):
    def __init__(self):
        GpioObj.__init__(self)

    def parse(self, node):
        log(LogLevel.info, 'GpioObj_whitney parse')
        GpioObj.parse(self, node)

    def gen_files(self):
        GpioObj.gen_files(self)

    def gen_spec(self, para):
        GpioObj.gen_spec(self, para)

    def is_i2cPadPin(self, name):
        return False

