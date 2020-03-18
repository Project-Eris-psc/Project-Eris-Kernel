#! /usr/bin/python
# -*- coding: utf-8 -*-

import re
import string
import xml.dom.minidom
import ConfigParser

from ModuleObj import ModuleObj
#from utility import util
from utility.util import sorted_key
from data.I2cData import I2cData
from data.I2cData import BusData
import ChipObj

class I2cObj(ModuleObj):
    def __init__(self):
        ModuleObj.__init__(self, 'cust_i2c.h', 'cust_i2c.dtsi')
        self.__busList = []
        self.__bBusEnable = True

    def get_cfgInfo(self):
        cp = ConfigParser.ConfigParser(allow_no_value=True)
        cp.read(ModuleObj.get_figPath())

        I2cData._i2c_count = string.atoi(cp.get('I2C', 'I2C_COUNT'))
        I2cData._channel_count = string.atoi(cp.get('I2C', 'CHANNEL_COUNT'))

        if cp.has_option('Chip Type', 'I2C_BUS'):
            flag = cp.get('Chip Type', 'I2C_BUS')
            if flag == '0':
                self.__bBusEnable = False

    def read(self, node):
        nodes = node.childNodes
        for node in nodes:
            if node.nodeType == xml.dom.minidom.Node.ELEMENT_NODE:
                if cmp(node.nodeName, 'count') == 0:
                    self.__count = node.childNodes[0].nodeValue
                    continue
                if node.nodeName.find('bus') != -1:
                    speedNode = node.getElementsByTagName('speed_kbps')
                    enableNode = node.getElementsByTagName('pullPushEn')

                    data = BusData()
                    if len(speedNode):
                        data.set_speed(speedNode[0].childNodes[0].nodeValue)
                    if len(enableNode):
                        data.set_enable(enableNode[0].childNodes[0].nodeValue)

                    self.__busList.append(data)
                elif node.nodeName.find('device') != -1:
                    nameNode = node.getElementsByTagName('varName')
                    channelNode = node.getElementsByTagName('channel')
                    addrNode = node.getElementsByTagName('address')

                    data = I2cData()
                    if len(nameNode):
                        data.set_varName(nameNode[0].childNodes[0].nodeValue)
                    if len(channelNode):
                        data.set_channel(channelNode[0].childNodes[0].nodeValue)
                    if len(addrNode):
                        data.set_address(addrNode[0].childNodes[0].nodeValue)

                    ModuleObj.set_data(self, node.nodeName, data)

        return True

    def parse(self, node):
        self.get_cfgInfo()
        self.read(node)

    def gen_files(self):
        ModuleObj.gen_files(self)

    def gen_spec(self, para):
        ModuleObj.gen_spec(self, para)

    def fill_hFile(self):
        gen_str = ''
        for i in range(0, I2cData._channel_count):
            gen_str += '''#define I2C_CHANNEL_%d\t\t\t%d\n''' %(i, i)

        gen_str += '''\n'''

        #sorted_lst = sorted(ModuleObj.get_data(self).keys(), key=compare)
        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            temp = ''
            if value.get_address().strip() == '':
                temp = 'TRUE'
            else:
                temp = 'FALSE'
            gen_str += '''#define I2C_%s_AUTO_DETECT\t\t\t%s\n''' %(value.get_varName(), temp)
            gen_str += '''#define I2C_%s_CHANNEL\t\t\t%s\n''' %(value.get_varName(), value.get_channel())
            gen_str += '''#define I2C_%s_SLAVE_7_BIT_ADDR\t\t%s\n''' %(value.get_varName(), value.get_address().upper())
            gen_str += '''\n'''

        return gen_str

    def fill_dtsiFile(self):
        gen_str = ''
        for i in range(0, I2cData._channel_count):
            if i >= len(self.__busList):
                break;
            gen_str += '''&i2c%d {\n''' %(i)
            gen_str += '''\t#address-cells = <1>;\n'''
            gen_str += '''\t#size-cells = <0>;\n'''


            if self.__bBusEnable:
                gen_str += '''\tclock-frequency = <%d>;\n''' %(string.atoi(self.__busList[i].get_speed()) * 1000)
                temp_str = ''

                if cmp(self.__busList[i].get_enable(), 'false') == 0:
                    temp_str = 'use-open-drain'
                elif cmp(self.__busList[i].get_enable(), 'true') == 0:
                    temp_str = 'use-push-pull'
                gen_str += '''\tmediatek,%s;\n''' %(temp_str)

            for key in sorted_key(ModuleObj.get_data(self).keys()):
                value = ModuleObj.get_data(self)[key]
                channel = 'I2C_CHANNEL_%d' %(i)
                if cmp(value.get_channel(), channel) == 0 and cmp(value.get_varName(), 'NC') != 0 and value.get_address().strip() != '':
                    gen_str += '''\t%s@%s {\n''' %(value.get_varName().lower(), value.get_address()[2:].lower())
                    gen_str += '''\t\tcompatible = \"mediatek,%s\";\n''' %(value.get_varName().lower())
                    gen_str += '''\t\treg = <%s>;\n''' %(value.get_address().lower())
                    gen_str += '''\t\tstatus = \"okay\";\n'''
                    gen_str += '''\t};\n\n'''

            gen_str += '''};\n\n'''

        return gen_str