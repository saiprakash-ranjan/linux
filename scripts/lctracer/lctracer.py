#!/usr/local/bin/python

######################################################################
#
#
# Copyright 2012,2013 Sony Corporation.
#
#
######################################################################

"""LCTracer viewer"""

import os
import sys
import re
import svgwrite
import struct
import math
import copy


#struct trace_entry {
#    u64 time;
#    u32 data;
#    u32 mode:2;
#    u32 policy:3;
#    u32 offset:11;
#    u32 prio:8;
#    u32 state:5;
#    u32 cpuid:3;
#    s16 tgid;
#    s16 ppid;
#    s16 npid;
#    char message[26];
#    char tname[16];
#} __attribute__((__packed__));
class LCTracer_Entry:
    '''LCTracer entry structure.'''
    def __init__(self, entry):
        self.time = entry[DATA_FMAT_TIME]
        self.data = entry[DATA_FMAT_DATA]
        self.mode = (entry[DATA_FMAT_OFFSET] >> 30 & 0x3)
        self.policy = (entry[DATA_FMAT_OFFSET] >> 27 & 0x7)
        self.offset = (entry[DATA_FMAT_OFFSET] >> 16 & 0x7ff)
        self.prio = (entry[DATA_FMAT_OFFSET] >> 8 & 0xff)
        self.state = (entry[DATA_FMAT_OFFSET] >> 3 & 0x1f)
        self.cpuid = (entry[DATA_FMAT_OFFSET] & 0x7)
        self.tgid = entry[DATA_FMAT_TGID]
        self.ppid = entry[DATA_FMAT_PPID]
        self.npid = entry[DATA_FMAT_NPID]
        self.message = entry[DATA_FMAT_NAME].split('\x00')[0]
        self.tname = entry[DATA_FMAT_TNAME].split('\x00')[0]
        self.cpu_ratio = 0
        self.per_cpu_ratio = [0, 0, 0, 0, 0, 0, 0, 0]
        try:
            self.tname.decode('ascii')
        except UnicodeDecodeError:
            self.tname = "\"%s\"" % "-".join(
                c.encode('hex') for c in self.tname)
            if self.ppid not in NON_ASCII_ERR_TNAME_DICT.keys():
                NON_ASCII_ERR_TNAME_DICT.update({self.ppid: self.tname})
        try:
            self.message.decode('ascii')
        except UnicodeDecodeError:
            self.message = "\"%s\"" % "-".join(
                c.encode('hex') for c in self.message)
            if self.mode == USER_INFO:
                if self.ppid not in NON_ASCII_ERR_USER_DICT.keys():
                    NON_ASCII_ERR_USER_DICT.update({self.ppid: self.message})
            elif self.mode == THREAD_INFO and self.state != 0:
                if self.ppid not in NON_ASCII_ERR_WCHAN_DICT.keys():
                    NON_ASCII_ERR_WCHAN_DICT.update({self.ppid: self.message})

        global POLICY_ERR_DICT
        if self.policy not in THREAD_POLICY_TYPE_DICT.keys() and \
            self.ppid not in POLICY_ERR_DICT:
            POLICY_ERR_DICT.update({self.ppid: str(self.policy)})
            print 'Warning: thread %d has unknown policy "%d" in binary file' \
                % (self.ppid, self.policy)

SCALE = 100
SVG_WIDTH = 800
SVG_HEIGHT = 1200
BASE_HEIGHT = 40
IOWAIT_HEIGHT = 220
ROW_HEIGHT = 20
NAME_WIDTH = 160
TGID_WIDTH = 40
PID_WIDTH = 40
PRIO_WIDTH = 40
POLICY_WIDTH = 100
CPU_RATIO_WIDTH = 110

# BASE_WIDTH includes below items.
# After getting CPU_NR, it should be updated.
BASE_WIDTH = NAME_WIDTH + TGID_WIDTH + PID_WIDTH +\
    PRIO_WIDTH + POLICY_WIDTH + CPU_RATIO_WIDTH + 10

START_TIME = 0
END_TIME = 0
RATE = 1
THREADS_LIST = {}
IRQS_LIST = {}
IRQ_DESC_DICT = {}
USERINFO_LIST = {}
TOTAL = 0
UNIT = 100
BINARY_DATA_LEN = 0
DATA_COUNT = 0

TIME_UNIT = 1000000
IRQ_DATA_UNIT = 1000

# separate thread0(swapper) for each CPU as different keys.
# base key is 0xf000.
# swapper/i is base key + cpuid, i means each CPU id.
SWAPPER_KEY_BASE = 0xf000

THREAD_INFO = 0
IRQ_INFO = 1
USER_INFO = 2
DATA_FMAT_TIME = 0
DATA_FMAT_DATA = 1
DATA_FMAT_OFFSET = 2
DATA_FMAT_TGID = 3
DATA_FMAT_PPID = 4
DATA_FMAT_NPID = 5
DATA_FMAT_NAME = 6
DATA_FMAT_TNAME = 7

IOWAIT_TIME_DATA = {}

PROCESS_DATA_SIZE = 64
BINARY_DATA = []

SHOW_TIME_INFO_FLAG = 0
SHOW_TID_IRQ_INFO_FLAG = 0
SHOW_FUNC_INFO_FLAG = 0
OUTPUT_FILE_FLAG = -1

THD_TAB_TGID_LIST = {}
THD_TAB_TNAME_LIST = {}
THD_TAB_PRIO_LIST = {}

SHOW_ALL_TID_MODE = 0
SHOW_ALL_IRQ_MODE = 0
SHOW_ALL_TGID_MODE = 0

IRQ_INPUT_LIST = []
TID_INPUT_LIST = []
TGID_INPUT_LIST = []

VERSION = ''
HZ = 0
CPU_NR = 1
MS_PER_SEC = 1000
MS_PER_JIFFIES = 0
IRQ_MIN_VAL_MS = 0.001
ENDIAN_INFO = ''
TITLE_NAME = ''
SYSTEM_UNAME = ''
SYSTEM_CPU = ''
BUFFER_SIZE = ''
THRESHOLD = ''
AUTOSTART = ''
STORE_TO_FILESYSTEM = ''
IOWAIT_SAMPLE_INTERVAL = ''
SAVE_PATH = ''
BUFFER_ADDRESS = ''
NON_ASCII_ERR_TNAME_DICT = {}
NON_ASCII_ERR_WCHAN_DICT = {}
NON_ASCII_ERR_USER_DICT = {}

POLICY_ERR_DICT = {}
THREAD_POLICY_TYPE_UNKNOWN = 0xff

SNSC_LCTRACER_IPI_IRQ = 0x1000
SNSC_LCTRACER_LOC_IRQ = 0x2000

TASK_STATE_BITMASK = {
    0: 'TASK_RUNNING', 1: 'TASK_INTERRUPTIBLE', 2: 'TASK_UNINTERRUPTIBLE',
    4: 'TASK_STOPPED', 8: 'TASK_TRACED', 16: 'TASK_DEAD'
}

THREAD_POLICY_TYPE_DICT = {
    0: 'NORMAL', 1: 'FIFO', 2: 'R.R.'
}

EARNING_MSG = '''[-b t1] [-e t2] [-i interval] [-s tid,irq,tgid]
     [-a|-d time|tgid|tid|prio|policy|ratio]
     [-o svg,SVG,txt] [-t tid,irq,all] [-w] input output
'''

BEGINNING_TIME_USAGE = '''
-b t1: Specify the beginning time shown in chart.
'''

END_TIME_USAGE = '''
-e t2: Specify the end time shown in chart.
'''

INTERVAL_TIME_USAGE = '''
-i: Specify the interval time(unit is millisecond) of time line shown in chart.
'''

SHOW_IRQ_TID_USAGE = '''
-s tid,irq,tgid: It means show thread or/and irq information.
'''

THREAD_SORT_USAGE = '''
-a time|tgid|tid|prio|policy|ratio: It means sort ascending by specified \
section.

-d time|tgid|tid|prio|policy|ratio: It means sort descending by specified \
section.
'''

SHOW_IIME_USAGE = '''
-t: It means show time information for specified thread or/and irq by
"-s" option.
'''
SHOW_WCHAN_USAGE = '''
-w: It means show wchan information for each thread in chart, including
    function name, address and offset, which the thread went to sleep.
'''
INPUT_USAGE = '''
input: Input is a directory name, which should include
    - lct_lctracer.dat:        measurement data file for thread and irq.
    - lct_iowait.log:          system iowait information.
    - lct_thread_table.log:    thread name and id table for application.
    - lct_measurement_env.log: kernel environment setting's information.
    - lct_irq_desc.log:        irq description table.
'''
OUTPUT_USAGE = '''
output: Specify the name of output, whose format is decided by "-o" option.
'''

OUTPUT_FILE_USAGE = '''
-o: Specify the output file type
    -o svg, output is svg format as Output_horizontal.svg
    -o SVG, output is svg format as Output_in_2_lines.svg
    -o txt, output is txt format
    If this option is not specified, it will be identical with "-o svg"
'''

CSS = '''
.Title {
    font-family: sans-serif;
    font-size: 14pt;
    font-weight: bold;
    fill: #000000;
}
.Title_stat {
    font-family: sans-serif;
    font-size: 10pt;
    font-weight: bold;
    fill: #000000;
}
.ProcessLabel {
    font-family: sans-serif;
    font-size: 9pt;
    fill: #000000;
}
.LegendTextBold {
    font-family: sans-serif;
    font-size: 9pt;
    font-weight: bold;
    fill: #000000;
}
.LegendText {
    font-family: sans-serif;
    font-size: 9pt;
    fill: #000000;
}
.Back {
    fill: white;
}
.Tick {
    stroke: #e6e6e6;
    stroke-width: 1px;
}
.AxisLabel {
    font-family: sans-serif;
    font-size: 9pt;
    fill: #000000;
    opacity: 0.7;
}
.Bold {
    stroke: #d0d0d0;
}

.CPU {
    fill-opacity: 0.25;
    fill: #668cb2;
    stroke: #668cb2;
}

.IO {
    fill-opacity: 0.25;
    fill: #ffff00;
    stroke: #ffff00;
}
.THRD1 {
    fill-opacity: 0.25;
    fill:   #6db266;
    stroke: #6db266;
}
.IRQ2 {
    fill-opacity: 0.5;
    fill:   #66b2b0;
    stroke: #66b2b0;
}
.THREAD{
    fill-opacity: 0.5;
    fill:   #ff0000;
    stroke: #ff0000;
}
.IRQ {
    fill-opacity: 0.5;
    fill:   #ff0000;
    stroke: #ff0000;
}
.IRQ1 {
    fill-opacity: 0.5;
    fill:   #00fa00;
    stroke: #00fa00;
}
.THRD {
    fill-opacity: 0.5;
    fill:   #8b008b;
    stroke: #8b008b;
}
.USER{
    fill-opacity: 0.5;
    fill:   #0000fb;
    stroke: #0000fb;
}
.IDLE{
    fill-opacity: 0.5;
    fill:   #ffff00;
    stroke: #66b2b0;
}
.Process {
    stroke: #668cb2;
    fill: none;
}
.ProcessStatus {
    stroke: none;
}
.Sleeping {
    fill: #fff200;
    fill-opacity: 0.25;
}
.Running {
    fill: #668cb2;
    fill-opacity: 0.25;
}
.CPU0{
    fill-opacity: 0.5;
    fill:   #ff0000;
    stroke: #ff0000;
}
.CPU1{
    fill-opacity: 0.5;
    fill:   #0fe7ef;
    stroke: #0fe7ef;
}
.CPU2{
    fill-opacity: 0.5;
    fill:   #df23dc;
    stroke: #df23dc;
}
.CPU3{
    fill-opacity: 0.5;
    fill:   #1cb512;
    stroke: #1cb512;
}
.CPU4{
    fill-opacity: 0.5;
    fill:   #df5f12;
    stroke: #df5f12;
}
.CPU5{
    fill-opacity: 0.5;
    fill:   #6f0fe3;
    stroke: #6f0fe3;
}
.CPU6{
    fill-opacity: 0.5;
    fill:   #7d7e00;
    stroke: #7d7e00;
}
.CPU7{
    fill-opacity: 0.5;
    fill:   #611100;
    stroke: #611100;
}
}
'''


def get_time(string_usage, strtime):
    """This function is used to change string to time"""

    time = 0
    if re.search(r'ms', strtime):
        beginning_time = re.sub(r'ms', '', strtime)
        try:
            time = float(beginning_time)
        except ValueError:
            print ValueError
            print 'Error: the %s time is invalid!' % string_usage
            sys.exit(ValueError)
    elif re.search(r's', strtime):
        beginning_time = re.sub(r's', '', strtime)
        try:
            time = float(beginning_time) * 1000
        except ValueError:
            print ValueError
            print 'Error: the %s time is invalid!' % string_usage
            sys.exit(ValueError)
    else:
        print 'Error: the %s time is invalid!' % string_usage
        sys.exit(ValueError)

    return time


def parse_parameter(arg_num, arg_value):
    """This function is used to parse parameter"""

    global START_TIME
    global END_TIME
    global UNIT
    global OUTPUT_FILE_FLAG
    global SHOW_TIME_INFO_FLAG
    global SHOW_TID_IRQ_INFO_FLAG
    global SHOW_FUNC_INFO_FLAG
    global TID_INPUT_LIST
    global TGID_INPUT_LIST
    global IRQ_INPUT_LIST
    global SHOW_ALL_TID_MODE
    global SHOW_ALL_IRQ_MODE
    global SHOW_ALL_TGID_MODE
    global THREAD_SORT_TYPE
    global THREAD_SORT_ORDER

    THREAD_SORT_TYPE = 0
    THREAD_SORT_ORDER = 0

    i = 0
    for i in range(i, arg_num):
        if arg_value[i] == '-b':
            START_TIME = get_time("beginning", arg_value[i + 1])
            if START_TIME < 0:
                print 'Error: beginning time is invalid!'
                sys.exit(ValueError)
        elif arg_value[i] == '-e':
            END_TIME = get_time("end", arg_value[i + 1])
            if END_TIME <= 0:
                print 'Error: end time is invalid!'
                sys.exit(ValueError)

        elif arg_value[i] == '-i':
            try:
                UNIT = float(arg_value[i + 1])
            except ValueError:
                print ValueError
                print 'Error: specify the interval time(UNIT is millisecond)!'
                print 'Usage:\n%s' % INTERVAL_TIME_USAGE
                sys.exit(ValueError)
            if UNIT < 0:
                print 'Error: specify the interval time(UNIT is millisecond)!'
                sys.exit(ValueError)

        elif arg_value[i] == '-a' or arg_value[i] == '-d':
            try:
                THREAD_SORT_TYPE = {
                    'time': 0,
                    'tgid': 1,
                    'tid': 2,
                    'prio': 3,
                    'policy': 4,
                    'ratio': 5,
                }[arg_value[i + 1]]
            except KeyError:
                print THREAD_SORT_USAGE
                sys.exit(ValueError)

            THREAD_SORT_ORDER = {
                '-a': 0,
                '-d': 1,
            }[arg_value[i]]

        elif arg_value[i] == '-s':
            tmp_list = arg_value[i + 1].split(',')
            error_parameter = 0
            if re.search(r'tid', arg_value[i + 1]):
                error_parameter = 1
                TID_INPUT_LIST = get_tid_irq_tgid_list('tid', tmp_list)
                if len(TID_INPUT_LIST) > 0:
                    SHOW_ALL_TID_MODE = 2
            if re.search(r'irq', arg_value[i + 1]):
                error_parameter = 1
                IRQ_INPUT_LIST = get_tid_irq_tgid_list('irq', tmp_list)
                if len(IRQ_INPUT_LIST) > 0:
                    SHOW_ALL_IRQ_MODE = 2
            if re.search(r'tgid', arg_value[i + 1]):
                error_parameter = 1
                TGID_INPUT_LIST = get_tid_irq_tgid_list('tgid', tmp_list)
                if len(TGID_INPUT_LIST) > 0:
                    SHOW_ALL_TGID_MODE = 2
            if error_parameter == 0:
                print 'Error: -s option is invalid!'
                sys.exit(ValueError)
            SHOW_TID_IRQ_INFO_FLAG = 1
        elif arg_value[i] == '-t':
            SHOW_TIME_INFO_FLAG = 1
        elif arg_value[i] == '-w':
            SHOW_FUNC_INFO_FLAG = 1
        elif arg_value[i] == '-o':
            if 'txt' == arg_value[i + 1]:
                OUTPUT_FILE_FLAG = 0
            elif 'svg' == arg_value[i + 1]:
                OUTPUT_FILE_FLAG = 1
            elif 'SVG' == arg_value[i + 1]:
                OUTPUT_FILE_FLAG = 2
            else:
                print 'Error: unknown type of output!'
                print 'Usage:\n %s' % OUTPUT_FILE_USAGE
                sys.exit(ValueError)
        elif arg_value[i] == '-h' or arg_value[i] == '--help' or arg_num < 3:
            print 'Usage: %s %s%s%s%s%s%s%s%s%s%s%s' % (
                arg_value[0], EARNING_MSG, BEGINNING_TIME_USAGE,
                END_TIME_USAGE, SHOW_IRQ_TID_USAGE,
                INTERVAL_TIME_USAGE, SHOW_IIME_USAGE, THREAD_SORT_USAGE,
                OUTPUT_FILE_USAGE, SHOW_WCHAN_USAGE, OUTPUT_USAGE,
                INPUT_USAGE)
            sys.exit(0)

        i = i + 1
    if OUTPUT_FILE_FLAG < 0:
        OUTPUT_FILE_FLAG = 1

    if START_TIME != 0 and END_TIME != 0 and START_TIME >= END_TIME:
        print 'Error: the beginning time is later than end time.'
        sys.exit(ValueError)


def get_tid_irq_tgid_list(string, input_list):
    """This function is used to get tig, tgid, irq through -s option"""

    tid_irq_list = []
    tgid_flag = 0
    global SHOW_ALL_TID_MODE
    global SHOW_ALL_IRQ_MODE
    global SHOW_ALL_TGID_MODE
    global SNSC_LCTRACER_IPI_IRQ
    global SNSC_LCTRACER_LOC_IRQ

    for item in input_list:
        if tgid_flag == 1:
            if re.search(r'[a-z]', item):
                if item == 'ipi':
                    item = '%s' % SNSC_LCTRACER_IPI_IRQ
                elif item == 'loc':
                    item = '%s' % SNSC_LCTRACER_LOC_IRQ
                else:
                    break
            tid_irq_list.append(int(item))

        if re.search(string, item):
            tgid_flag = 1
            tmpid = item.split('=')
            if len(tmpid) == 1:
                if string == 'irq':
                    SHOW_ALL_IRQ_MODE = 1
                elif string == 'tid':
                    SHOW_ALL_TID_MODE = 1
                else:
                    SHOW_ALL_TGID_MODE = 1
                break
            else:
                try:
                    if tmpid[1] == 'ipi':
                        input_item = SNSC_LCTRACER_IPI_IRQ
                    elif tmpid[1] == 'loc':
                        input_item = SNSC_LCTRACER_LOC_IRQ
                    else:
                        input_item = int(tmpid[1])
                    if input_item < 0:
                        print 'Error: -s option is invalid!'
                        print '%s is negative' % string
                        sys.exit(ValueError)
                    tid_irq_list.append(input_item)
                except ValueError:
                    print ValueError
                    print 'Error: -s option is invalid!'
                    print SHOW_IRQ_TID_USAGE
                    sys.exit(ValueError)
    return tid_irq_list


def get_iowait_data(iowait_log):
    """This function is used to get iowait information through iowait log"""

    if not os.path.isfile(iowait_log):
        print 'Warning: File %s does not exist!' % iowait_log
        return

    file_obj = open(iowait_log, 'r')

    time = 0
    lines = file_obj.readlines()
    while len(lines) > 0:
        line = lines.pop(0)
        if re.search(r'^\[', line):
            try:
                time = float(line.split()[1])
            except ValueError:
                print ValueError
            line = lines.pop(0)
            data = line.split()
            # The fifth value is iowait
            if len(data) > 5:
                IOWAIT_TIME_DATA.update({time: data[5]})
            else:
                print 'Warning: no iowait information at %s Sec!' % time

    file_obj.close()

    return IOWAIT_TIME_DATA


def get_binary_data(lct_lctracer_dat):
    """This function is used get binary through lct_lctracer.dat"""

    if not os.path.isfile(lct_lctracer_dat):
        print 'Error: File %s does not exist!' % lct_lctracer_dat
        sys.exit(IOError)

    global BINARY_DATA
    global BINARY_DATA_LEN
    global THREADS_LIST
    global IRQS_LIST
    global USERINFO_LIST

    irqs = []
    threads = []
    userinfo = []
    BINARY_DATA = []

    file_desc = open(lct_lctracer_dat, 'rb')
    while file_desc is not None:
        tmpdata = file_desc.read(PROCESS_DATA_SIZE)
        if len(tmpdata) < PROCESS_DATA_SIZE:
            break
        if ENDIAN_INFO == 'little endian':
            tmp_data = struct.unpack('<1Q2I3h26s16s', tmpdata)
        else:
            tmp_data = struct.unpack('>1Q2I3h26s16s', tmpdata)
        data = list(tmp_data)
        trace_entry = LCTracer_Entry(data)
        if trace_entry.mode == IRQ_INFO:
            irqs.append(trace_entry)
        elif trace_entry.mode == THREAD_INFO:
            threads.append(trace_entry)
        else:
            userinfo.append(trace_entry)
        BINARY_DATA_LEN += 1
        BINARY_DATA.append(trace_entry)
    file_desc.close()

    THREADS_LIST = get_thread_list(threads)

    IRQS_LIST = get_irqs_list(irqs)

    USERINFO_LIST = get_useinfo_list(userinfo)

    parse_binary_data()

    return (THREADS_LIST, IRQS_LIST)


def get_useinfo_list(userinfo):
    """This function is used to get user entry information"""

    userlenth = len(userinfo)
    if userlenth <= 0:
        return

    userlist = {}
    i = 0
    for i in range(i, userlenth):
        ppid = userinfo[i].ppid
        ppid_user_list = []
        if ppid in userlist.keys():
            ppid_user_list = userlist[ppid]
        ppid_user_list.append(userinfo[i])
        userlist.update({ppid: ppid_user_list})

    return userlist


def get_header_info(header_file_name):
    """This function is used to get header information"""

    if not os.path.isfile(header_file_name):
        print 'Error: File %s does not exist!' % header_file_name
        sys.exit(IOError)

    header_fd = open(header_file_name, 'r')

    global VERSION
    global HZ
    global CPU_NR
    global MS_PER_JIFFIES
    global ENDIAN_INFO
    global TITLE_NAME
    global SYSTEM_UNAME
    global SYSTEM_CPU
    global BUFFER_SIZE
    global THRESHOLD
    global AUTOSTART
    global STORE_TO_FILESYSTEM
    global IOWAIT_SAMPLE_INTERVAL
    global SAVE_PATH
    global BUFFER_ADDRESS
    global SNSC_LCTRACER_IPI_IRQ
    global SNSC_LCTRACER_LOC_IRQ

    for line in header_fd.readlines():
        if re.search(r'Linux', line):
            SYSTEM_UNAME = line.split('\n')[0]
        elif re.search(r'version', line):
            VERSION = (line.split(':')[1]).strip().split('.')
        elif re.search(r'Endian', line):
            ENDIAN_INFO = (line.split(':')[1]).strip()
        elif re.search(r'HZ', line):
            HZ = int((line.split(':')[1]).strip())
            MS_PER_JIFFIES = MS_PER_SEC / HZ
        elif re.search(r'SNSC_LCTRACER_IPI_IRQ', line):
            SNSC_LCTRACER_IPI_IRQ = int((line.split(':')[1]).strip())
        elif re.search(r'SNSC_LCTRACER_LOC_IRQ', line):
            SNSC_LCTRACER_LOC_IRQ = int((line.split(':')[1]).strip())
        elif re.search(r'CPU number', line):
            CPU_NR = int((line.split(':')[1]).strip())
        elif re.search(r'system type', line) or re.search(r'Hardware', line):
            TITLE_NAME = line.split(':')[1]
        elif re.search(r'cpu model', line) or re.search(r'Processor', line):
            SYSTEM_CPU = line.split(':')[1]
        elif re.search(r'buffer size', line):
            BUFFER_SIZE = line.split(':')[1]
        elif re.search(r'threshold', line):
            THRESHOLD = line.split(':')[1]
        elif re.search(r'autostart', line):
            AUTOSTART = line.split(':')[1]
        elif re.search(r'storing to filesystem', line):
            STORE_TO_FILESYSTEM = line.split(':')[1]
        elif re.search(r'iowait', line):
            IOWAIT_SAMPLE_INTERVAL = line.split(':')[1]
        elif re.search(r'saving path', line):
            SAVE_PATH = line.split(':')[1]
        elif re.search(r'buffer address', line):
            BUFFER_ADDRESS = line.split(':')[1]

    header_fd.close()

    return


def get_irq_desc_table(irq_desc_name):
    """This function is used to get irq description"""

    global IRQ_DESC_DICT

    if not os.path.isfile(irq_desc_name):
        print 'Error: File %s does not exist!' % irq_desc_name
        sys.exit(IOError)

    irq_desc_fd = open(irq_desc_name, 'r')

    # The format of irq counter is "%10u " for each CPU.
    # So the length of irq counter per CPU is 11.
    len_irq_cnt_per_cpu = 11
    for line in irq_desc_fd.readlines()[1:]:
        irq_num = line.split(':')[0]

        irq_info_index = len(irq_num) + len(': ') + \
            len_irq_cnt_per_cpu * CPU_NR
        irq_info = line[irq_info_index:].strip()
        if not len(irq_info):
            continue

        irq_desc = re.split(r'\s\s+', irq_info)[-1]
        IRQ_DESC_DICT.update({irq_num.strip(): irq_desc})

    irq_desc_fd.close()

    return


def get_thread_list(threads):
    """This function is used to get thread information"""

    thread_lenth = len(threads)
    if thread_lenth <= 0:
        print 'Warning: thread data is NULL!'
        return

    global DATA_COUNT
    threadslist = {}
    i = 0
    for i in range(i, thread_lenth):
        ppid = threads[i].ppid
        ppid_list = []
        if ppid not in threadslist.keys():
            DATA_COUNT += 1
        else:
            ppid_list = threadslist[ppid]
        ppid_list.append(threads[i])
        threadslist.update({ppid: ppid_list})

        npid = threads[i].npid
        npid_list = []
        if npid not in threadslist.keys():
            DATA_COUNT += 1
        else:
            npid_list = threadslist[npid]
        npid_list.append(threads[i])
        threadslist.update({npid: npid_list})

    return threadslist


def get_irqs_list(irqs):
    """this function is used to get irq information"""

    irqs_len = len(irqs)
    if irqs_len <= 0:
        print 'Warning: IRQ data is NULL!'
        return

    i = 0
    irqslist = {}
    global DATA_COUNT

    for i in range(i, irqs_len):
        irq_id = irqs[i].data & 0xffff
        tmp_list = []
        if irq_id in irqslist.keys():
            tmp_list = irqslist[irq_id]
        else:
            DATA_COUNT += 1

        tmp_list.append(irqs[i])
        irqslist.update({irq_id: tmp_list})

    return irqslist


def parse_binary_data():
    """This function is used to parse binary data,
    get the svg width and heigh and so on"""

    if DATA_COUNT <= 0:
        print 'Warning: binary data is NULL!'
        return

    global START_TIME
    global END_TIME
    global RATE
    global TOTAL
    global SVG_WIDTH
    global SVG_HEIGHT
    global BASE_WIDTH

    # If does not specify the beginning time, set the first BINARY data time
    if START_TIME == 0:
        START_TIME = float(BINARY_DATA[0].time) / TIME_UNIT
    elif START_TIME < float(BINARY_DATA[0].time) / TIME_UNIT:
        print '''
            Warning: the beginning time is earlier than
            the first data time. Use the first data time.'''
        START_TIME = float(BINARY_DATA[0].time) / TIME_UNIT

    # If does not specify the end time, set the last BINARY data time
    if END_TIME == 0:
        END_TIME = float(BINARY_DATA[BINARY_DATA_LEN - 1].time) / TIME_UNIT
    elif END_TIME > float(BINARY_DATA[BINARY_DATA_LEN - 1].time) / TIME_UNIT:
        print '''
            Warning: the end time is later than
            the last data time. Use the last data time.'''
        END_TIME = float(BINARY_DATA[BINARY_DATA_LEN - 1].time) / TIME_UNIT

    if END_TIME <= START_TIME:
        print 'Error: the end time is earlier than beginning time!'
        sys.exit(ValueError)

    # If TOTAL <= 0: How to set TOTAL?
    TOTAL = int(float(END_TIME - START_TIME) / UNIT)
    if TOTAL <= 0:
        TOTAL = 1

    RATE = SCALE / UNIT

    BASE_WIDTH = NAME_WIDTH + TGID_WIDTH + PID_WIDTH + \
        PRIO_WIDTH + POLICY_WIDTH + CPU_RATIO_WIDTH * CPU_NR + 10

    if OUTPUT_FILE_FLAG == 1:
        SVG_WIDTH = BASE_WIDTH + RATE * (END_TIME - START_TIME) - 8
        SVG_HEIGHT = (DATA_COUNT + CPU_NR - 1) * ROW_HEIGHT + \
            BASE_HEIGHT + IOWAIT_HEIGHT
        TOTAL = int((SVG_WIDTH - BASE_WIDTH) / SCALE)
    elif OUTPUT_FILE_FLAG == 2:
        SVG_WIDTH = RATE * (END_TIME - START_TIME) + NAME_WIDTH
        SVG_HEIGHT = 3 * ROW_HEIGHT + BASE_HEIGHT + IOWAIT_HEIGHT
        TOTAL = int((SVG_WIDTH - NAME_WIDTH) / SCALE)
    return


def parse_log(input_dir):
    """This function is read input directory"""

    if not os.path.exists(input_dir):
        print 'Error: file path %s does not exist!' % input_dir
        sys.exit(IOError)

    get_header_info(input_dir + '/' + 'lct_measurement_env.log')
    get_irq_desc_table(input_dir + '/' + 'lct_irq_desc.log')
    get_binary_data(input_dir + '/' + 'lct_lctracer.dat')
    get_iowait_data(input_dir + '/' + 'lct_iowait.log')
    get_thread_table(input_dir + '/' + 'lct_thread_table.log')

    return


def draw_header(dwg, table):
    """This function is used to draw header information to svg"""

    global BASE_HEIGHT
    global CPU_NR

    table_group = table.add(dwg.g(transform='translate(0, -30)'))
    header_row_height = 16
    header_max_width = 160
    table_group.add(dwg.text(
        'LCTracer chart(V%s.%s):%s' % (VERSION[0], VERSION[1], TITLE_NAME),
        dx=[2], dy=[5], class_='Title'))
    table_group.add(dwg.text(
        'System uname: ', dx=[2], dy=[5 + header_row_height],
        class_='LegendText'))
    table_group.add(dwg.text(
        '%s' % SYSTEM_UNAME, dx=[header_max_width],
        dy=[5 + header_row_height], class_='LegendText'))
    table_group.add(dwg.text(
        'CPU: ', dx=[2], dy=[5 + 2 * header_row_height],
        class_='LegendText'))
    table_group.add(dwg.text(
        '%s' % SYSTEM_CPU, dx=[header_max_width],
        dy=[5 + 2 * header_row_height], class_='LegendText'))
    table_group.add(dwg.text(
        'CPU number: ', dx=[2], dy=[5 + 3 * header_row_height],
        class_='LegendText'))
    table_group.add(dwg.text(
        '%s' % CPU_NR, dx=[header_max_width],
        dy=[5 + 3 * header_row_height], class_='LegendText'))
    table_group.add(dwg.text(
        'LCTracer parameters state:', dx=[2],
        dy=[5 + 4 * header_row_height], class_='Title_stat'))
    table_group.add(dwg.text(
        'Buffer size: ', dx=[2], dy=[5 + 5 * header_row_height],
        class_='LegendText'))
    table_group.add(dwg.text(
        '%s' % BUFFER_SIZE, dx=[header_max_width],
        dy=[5 + 5 * header_row_height], class_='LegendText'))
    table_group.add(dwg.text(
        'Threshold: ', dx=[2], dy=[5 + 6 * header_row_height],
        class_='LegendText'))
    table_group.add(dwg.text(
        '%s' % (THRESHOLD), dx=[header_max_width],
        dy=[5 + 6 * header_row_height], class_='LegendText'))
    table_group.add(dwg.text(
        'Autostart: ', dx=[2], dy=[5 + 7 * header_row_height],
        class_='LegendText'))
    table_group.add(dwg.text(
        '%s' % AUTOSTART, dx=[header_max_width],
        dy=[5 + 7 * header_row_height], class_='LegendText'))
    table_group.add(dwg.text(
        'Store to filesystem: ',
        dx=[2], dy=[5 + 8 * header_row_height], class_='LegendText'))
    table_group.add(dwg.text('%s' % STORE_TO_FILESYSTEM, dx=[header_max_width],
                    dy=[5 + 8 * header_row_height], class_='LegendText'))
    table_group.add(dwg.text('IO wait sample interval: ', dx=[2],
                    dy=[5 + 9 * header_row_height], class_='LegendText'))
    table_group.add(dwg.text(
        '%s' % IOWAIT_SAMPLE_INTERVAL, dx=[header_max_width],
        dy=[5 + 9 * header_row_height], class_='LegendText'))
    table_group.add(dwg.text(
        'Saving path: ', dx=[2], dy=[5 + 10 * header_row_height],
        class_='LegendText'))
    table_group.add(dwg.text(
        '%s' % SAVE_PATH, dx=[header_max_width],
        dy=[5 + 10 * header_row_height], class_='LegendText'))
    table_group.add(dwg.text(
        'Buffer address: ', dx=[2],
        dy=[5 + 11 * header_row_height], class_='LegendText'))
    table_group.add(dwg.text(
        '%s' % BUFFER_ADDRESS, dx=[header_max_width],
        dy=[5 + 11 * header_row_height], class_='LegendText'))

    BASE_HEIGHT = header_row_height * 12 + ROW_HEIGHT

    table_group.add(dwg.rect(
        insert=(2, BASE_HEIGHT - ROW_HEIGHT), size=(10, 10), class_="IO"))
    table_group.add(dwg.text(
        "I/O (wait)", dx=[18],  dy=[BASE_HEIGHT - 10], class_='LegendText'))
    text_y = BASE_HEIGHT + IOWAIT_HEIGHT + 10
    rect_y = BASE_HEIGHT + IOWAIT_HEIGHT

    if OUTPUT_FILE_FLAG == 1:
        i = 0
        for i in range(i, CPU_NR):
            cpuid_class = 'CPU%d' % i
            cpu_legend_text = 'Running on %s' % cpuid_class
            table_group.add(dwg.rect(
                insert=(10, rect_y - (CPU_NR - i) * ROW_HEIGHT),
                size=(10, 10), class_=cpuid_class))
            table_group.add(dwg.text(
                cpu_legend_text, dx=[25],
                dy=[text_y - (CPU_NR - i) * ROW_HEIGHT],
                class_='LegendText'))

        table_group.add(dwg.rect(
            insert=(10, rect_y), size=(10, 10), class_="USER"))
        table_group.add(dwg.text(
            "User log", dx=[25], dy=[text_y], class_='LegendText'))
    else:
        table_group.add(dwg.rect(
            insert=(10, rect_y), size=(10, 10), class_="THRD"))
        table_group.add(dwg.text(
            "Thread running", dx=[25],  dy=[text_y], class_='LegendText'))
        table_group.add(dwg.rect(
            insert=(150, rect_y), size=(10, 10), class_="THRD1"))
        table_group.add(dwg.text(
            "Thread running", dx=[163], dy=[text_y], class_='LegendText'))
        table_group.add(dwg.rect(
            insert=(300, rect_y), size=(10, 10), class_="IRQ"))
        table_group.add(dwg.text(
            "ISR running", dx=[313], dy=[text_y], class_='LegendText'))
        table_group.add(dwg.rect(
            insert=(450, rect_y), size=(10, 10), class_="IRQ1"))
        table_group.add(dwg.text(
            "ISR running", dx=[463], dy=[text_y], class_='LegendText'))
        table_group.add(dwg.rect(
            insert=(600, rect_y), size=(10, 10), class_="USER"))
        table_group.add(dwg.text(
            "User-defined", dx=[613], dy=[text_y], class_='LegendText'))
    return table_group


def draw_io_wait(dwg, table):
    """This functuion is used to draw io wait information to svg"""

    global MS_PER_JIFFIES

    items = IOWAIT_TIME_DATA.items()
    items.sort()
    iowait_data = []
    iowait_data_max = 0
    for item in items:
        tmp_iowait_list = []
        try:
            time = item[0] * 1000
            start = (time - START_TIME) * RATE + BASE_WIDTH
        except ValueError:
            print ValueError
            return
        if time >= START_TIME and time < END_TIME:
            if len(iowait_data) > 0 and \
                    (int(item[1]) - int(iowait_data[-1][1])) > iowait_data_max:
                iowait_data_max = int(item[1]) - int(iowait_data[-1][1])
            tmp_iowait_list.append(start)
            tmp_iowait_list.append(item[1])
            iowait_data.append(tmp_iowait_list)
        elif time > END_TIME:
            break

    max_time = iowait_data_max * MS_PER_JIFFIES
    if max_time == 0:
        max_time = 10

    # there are totally 10 scales for iowait chart
    ms_per_scale = max_time / 10.0

    i = 1
    iowait_points = []
    iowait_data_len = len(iowait_data)
    iowait_hight = []
    iowait_low = []
    for i in range(i, iowait_data_len):
        tmp_hight = []
        tmp_low = []
        y_size_two = int(iowait_data[i][1]) * MS_PER_JIFFIES
        y_size_two = float(y_size_two) / ms_per_scale * ROW_HEIGHT
        y_size = int(iowait_data[i - 1][1]) * MS_PER_JIFFIES
        y_size = float(y_size) / ms_per_scale * ROW_HEIGHT
        y_line = ((
            BASE_HEIGHT + IOWAIT_HEIGHT - ROW_HEIGHT)) -\
            (y_size_two - y_size)

        x_line = iowait_data[i][0]
        if i == 1:
            iowait_hight.append(
                [x_line, (BASE_HEIGHT + IOWAIT_HEIGHT - ROW_HEIGHT)])
        tmp_hight.append(x_line)
        tmp_hight.append(y_line)
        tmp_low.append(x_line)
        tmp_low.append((BASE_HEIGHT + IOWAIT_HEIGHT - ROW_HEIGHT))
        iowait_hight.append(tmp_hight)
        iowait_low.append(tmp_low)

    i = 0
    line = table.add(dwg.g(class_='Tick', transform='translate(0.5, 0.5)'))
    y_size = BASE_HEIGHT - ROW_HEIGHT
    y_size_two = BASE_HEIGHT + IOWAIT_HEIGHT - ROW_HEIGHT
    for i in range(i, TOTAL + 1):
        if OUTPUT_FILE_FLAG == 1:
            line.add(dwg.line(
                start=(BASE_WIDTH + i * SCALE, y_size),
                end=(BASE_WIDTH + i * SCALE, y_size_two), class_='Bold'))
        elif OUTPUT_FILE_FLAG == 2:
            line.add(dwg.line(
                start=(NAME_WIDTH + i * SCALE, y_size),
                end=(NAME_WIDTH + i * SCALE, y_size_two), class_='Bold'))
    i = 0
    for i in range(i, 11):
        if OUTPUT_FILE_FLAG == 1:
            line.add(dwg.line(
                start=(BASE_WIDTH, BASE_HEIGHT + (i - 1) * ROW_HEIGHT),
                end=(SVG_WIDTH + 8, BASE_HEIGHT + (i - 1) * ROW_HEIGHT),
                class_='Bold'))
        elif OUTPUT_FILE_FLAG == 2:
            line.add(dwg.line(
                start=(NAME_WIDTH, BASE_HEIGHT + (i - 1) * ROW_HEIGHT),
                end=(SVG_WIDTH, BASE_HEIGHT + (i - 1) * ROW_HEIGHT),
                class_='Bold'))
        table.add(dwg.text(
            '%sms' % (i * ms_per_scale), class_='AxisLabel',
            dx=[BASE_WIDTH - 65],
            dy=[BASE_HEIGHT + IOWAIT_HEIGHT - (i + 1) * ROW_HEIGHT]))

    if len(iowait_hight) > 0:
        iowait_points = iowait_hight + sorted(iowait_low, reverse=True)
        pyline = dwg.polyline(class_='IO')

        pyline.points.extend(iowait_points)
        table.add(pyline)

    return


def draw_coordinate(dwg, table):
    """This function is used to draw coordinate to svg"""

    line = table.add(dwg.g(class_='Tick', transform='translate(0.5, 0.5)'))
    line.add(dwg.line(
        start=(BASE_WIDTH, BASE_HEIGHT + IOWAIT_HEIGHT),
        end=(BASE_WIDTH, BASE_HEIGHT + IOWAIT_HEIGHT - 3), class_='Bold'))

    i = 1
    for i in range(i, TOTAL + 1):
        if OUTPUT_FILE_FLAG == 1:
            line.add(dwg.line(
                start=(BASE_WIDTH + i * SCALE, BASE_HEIGHT + IOWAIT_HEIGHT),
                end=(BASE_WIDTH + i * SCALE, BASE_HEIGHT + IOWAIT_HEIGHT - 3),
                class_='Bold'))
        elif OUTPUT_FILE_FLAG == 2:
            line.add(dwg.line(
                start=(BASE_WIDTH + i * SCALE, BASE_HEIGHT + IOWAIT_HEIGHT),
                end=(NAME_WIDTH + i * SCALE, BASE_HEIGHT + IOWAIT_HEIGHT - 3),
                class_='Bold'))

    group = table.add(dwg.g(transform='translate(0, 10)'))
    i = 0
    for i in (range(i, (TOTAL + 1))):
        if i % 5 == 0:
            time = '%0.6f' % (i * UNIT + START_TIME)
            time_len = (len(time) - 1) * 4
            group.add(dwg.text(
                '%sms' % time, class_='AxisLabel',
                dx=[i * SCALE + BASE_WIDTH - time_len],
                dy=[BASE_HEIGHT + IOWAIT_HEIGHT - 15]))

    return


def draw_thread_body(
    dwg, group, key, i, thread_name_begin, tgid_begin,
        pid_begin, prio_begin, policy_begin, cpu_ratio_begin, thread):
    """This function is used to draw thread body to svg"""

    global NON_ASCII_ERR_TNAME_DICT
    global NON_ASCII_ERR_WCHAN_DICT
    global NON_ASCII_ERR_USER_DICT

    tgid = -1
    thread_name = ''
    prio = -1
    draw_data_count = 0
    x_two_size = BASE_HEIGHT + IOWAIT_HEIGHT + i * ROW_HEIGHT
    line_start = x_two_size - 15
    line_end = line_start + ROW_HEIGHT
    table_group = group.add(dwg.g())

    j = 1
    thrd_lenth = len(thread)
    background_drawn = False
    for j in range(j, thrd_lenth):
        tid = get_tid_by_key(key)
        if tid != thread[j - 1].npid or tid != thread[j].ppid:
            continue

        start = 0
        end = 0
        start = float(thread[j - 1].time) / TIME_UNIT

        if tid == thread[j].npid and thread[j].mode == IRQ_INFO:
            end = float(thread[j].time) / TIME_UNIT\
                - float(thread[j].data & 0xffff) / IRQ_DATA_UNIT
        else:
            end = float(thread[j].time) / TIME_UNIT

        if end == 0 or end < start:
            continue
        elif end >= END_TIME:
            if start < END_TIME:
                end = END_TIME
            else:
                break
        if start <= START_TIME:
            if end > START_TIME:
                start = START_TIME
            else:
                continue

        tname_class = 'ProcessLabel'
        user_class = 'ProcessLabel'
        wchan_class = 'ProcessLabel'
        if background_drawn is False and (i % 2) != 0:
            table_group.add(dwg.rect(
                class_='Sleeping', insert=(8, line_start),
                size=(SVG_WIDTH, ROW_HEIGHT)))
            background_drawn = True

        width = end - start
        cpuid = thread[j - 1].cpuid
        cpuid_class = 'CPU%d' % cpuid

        table_group.add(dwg.rect(
            class_=cpuid_class,
            insert=((start - START_TIME) * RATE + BASE_WIDTH, x_two_size - 15),
            size=(width * RATE, ROW_HEIGHT)))
        # user defined flag
        if USERINFO_LIST is not None and tid in USERINFO_LIST.keys():
            userinfo = USERINFO_LIST[tid]
            user_index = 0
            userinfo_len = len(userinfo)
            for user_index in range(user_index, userinfo_len):
                usr_start = float(userinfo[user_index].time) / TIME_UNIT
                if usr_start >= START_TIME and usr_start <= END_TIME:
                    table_group.add(dwg.rect(class_='USER', insert=((
                        usr_start - START_TIME) * RATE + BASE_WIDTH,
                        x_two_size - 15), size=(1, ROW_HEIGHT)))
                    if SHOW_FUNC_INFO_FLAG == 1:
                        table_group.add(dwg.text(
                            userinfo[user_index].message,
                            dx=[(usr_start - START_TIME) * RATE + BASE_WIDTH],
                            dy=[x_two_size], class_=user_class))

        draw_data_count += 1
        if SHOW_FUNC_INFO_FLAG == 1 and thread[j].mode == THREAD_INFO:
            if thread[j].state != 0 and thread[j].ppid != thread[j].npid:
                thread_function = '%s+%x(%x)' % (
                    thread[j].message,
                    thread[j].data,
                    thread[j].offset)
                table_group.add(dwg.text(thread_function, dx=[(
                    start - START_TIME) * RATE + BASE_WIDTH],
                    dy=[x_two_size], class_=wchan_class))

        if SHOW_TIME_INFO_FLAG == 1 and thread[j].mode == THREAD_INFO:
            table_group.add(dwg.text(
                '%0.6f' % start,
                dx=[(start - START_TIME) * RATE + BASE_WIDTH],
                dy=[x_two_size], class_='ProcessLabel'))
            table_group.add(dwg.text(
                '%0.6f' % end,
                dx=[(start - START_TIME) * RATE + BASE_WIDTH + width * RATE],
                y=[x_two_size], class_='ProcessLabel'))

    if draw_data_count > 0:
        thread_name = thread[-1].tname
        if tid == 0:
            thread_name = '%s/%d' % (thread[-1].tname, thread[-1].cpuid)
        table_group.add(dwg.text(
            thread_name, dx=[10], dy=[x_two_size], class_=tname_class))
        table_group.add(dwg.line(
            class_='Bold', start=(thread_name_begin, line_start),
            end=(thread_name_begin, line_end)))
        # set tgid
        tgid = thread[-1].tgid
        table_group.add(dwg.text(
            '%d' % tgid, dx=[2 + thread_name_begin],
            dy=[x_two_size], class_='ProcessLabel'))
        table_group.add(dwg.line(
            class_='Bold', start=(tgid_begin, line_start),
            end=(tgid_begin, line_end)))
        table_group.add(dwg.text(
            '%d' % tid, dx=[4 + tgid_begin], dy=[x_two_size],
            class_='ProcessLabel'))
        table_group.add(dwg.line(
            class_='Bold', start=(pid_begin, line_start),
            end=(pid_begin, line_end)))
        # set priority
        prio = thread[-1].prio
        table_group.add(dwg.text(
            '%d' % prio, dx=[6 + pid_begin], dy=[x_two_size],
            class_='ProcessLabel'))
        table_group.add(dwg.line(
            class_='Bold', start=(prio_begin, line_start),
            end=(prio_begin, line_end)))
        # set policy
        policy = thread[-1].policy
        if policy in THREAD_POLICY_TYPE_DICT.keys():
            ppolicy = THREAD_POLICY_TYPE_DICT.get(policy)
        else:
            ppolicy = 'UNKNOWN'
        table_group.add(dwg.text(
            ppolicy, dx=[6 + prio_begin], dy=[x_two_size],
            class_='ProcessLabel'))
        table_group.add(dwg.line(
            class_='Bold', start=(policy_begin, line_start),
            end=(policy_begin, line_end)))
        table_group.add(dwg.rect(
            class_='Process', insert=(8, line_start),
            size=(SVG_WIDTH, ROW_HEIGHT)))
        for cpuid in range(0, CPU_NR):
            table_group.add(dwg.text(
                '%f' % thread[-1].per_cpu_ratio[cpuid],
                dx=[8 + policy_begin + cpuid * CPU_RATIO_WIDTH],
                dy=[x_two_size],
                class_='ProcessLabel'))
            table_group.add(dwg.line(
                class_='Bold',
                start=(cpu_ratio_begin + cpuid * CPU_RATIO_WIDTH, line_start),
                end=(cpu_ratio_begin + cpuid * CPU_RATIO_WIDTH, line_end)))
    return draw_data_count


def draw_thread(dwg, group, i, final_thd_list):
    """This function is used to get thread information and draw thread, irq"""

    global CPU_NR
    thread_name_begin = 10 + NAME_WIDTH
    tgid_begin = thread_name_begin + TGID_WIDTH
    pid_begin = tgid_begin + PID_WIDTH
    prio_begin = pid_begin + PRIO_WIDTH
    policy_begin = prio_begin + POLICY_WIDTH
    cpu_ratio_begin = policy_begin + CPU_RATIO_WIDTH
    line_end = IOWAIT_HEIGHT + BASE_HEIGHT
    line_start = line_end - 15

    list_len = len(final_thd_list)
    j = 0
    for j in range(j, list_len):
        count = draw_thread_body(
            dwg, group, final_thd_list[j][0], i, thread_name_begin,
            tgid_begin, pid_begin, prio_begin, policy_begin,
            cpu_ratio_begin, final_thd_list[j][1])
        if count > 0:
            if final_thd_list[j][0] == 0:
                i += CPU_NR
            else:
                i += 1
    if i > 1:
        group.add(dwg.text(
            'name', dx=[10], dy=[line_end], class_='LegendTextBold'))
        group.add(dwg.line(
            class_='Bold', start=(thread_name_begin, line_start),
            end=(thread_name_begin, line_end + 5)))
        group.add(dwg.text(
            'tgid', dx=[2 + thread_name_begin], dy=[line_end],
            class_='LegendTextBold'))
        group.add(dwg.line(
            class_='Bold', start=(tgid_begin, line_start),
            end=(tgid_begin, line_end + 5)))
        group.add(dwg.text(
            'pid', dx=[4 + tgid_begin], dy=[line_end],
            class_='LegendTextBold'))
        group.add(dwg.line(
            class_='Bold', start=(pid_begin, line_start),
            end=(pid_begin, line_end + 5)))
        group.add(dwg.text(
            'prio', dx=[6 + pid_begin], dy=[line_end],
            class_='LegendTextBold'))
        group.add(dwg.line(
            class_='Bold', start=(prio_begin, line_start),
            end=(prio_begin, line_end + 5)))
        group.add(dwg.text(
            'policy', dx=[6 + prio_begin], dy=[line_end],
            class_='LegendTextBold'))
        group.add(dwg.line(
            class_='Bold', start=(policy_begin, line_start),
            end=(policy_begin, line_end + 5)))
        for cpuid in range(0, CPU_NR):
            group.add(dwg.text(
                'CPU%d ratio(%%)' % cpuid,
                dx=[8 + policy_begin + cpuid * CPU_RATIO_WIDTH],
                dy=[line_end],
                class_='LegendTextBold'))
            group.add(dwg.line(
                class_='Bold',
                start=(cpu_ratio_begin + cpuid * CPU_RATIO_WIDTH, line_start),
                end=(cpu_ratio_begin + cpuid * CPU_RATIO_WIDTH, line_end + 5)))
        group.add(dwg.rect(
            class_='Process', insert=(8, line_start),
            size=(SVG_WIDTH, ROW_HEIGHT)))
    return i


def get_finally_irqs_list(input_list):
    """This function is used get finally irq information to list"""

    finally_list = {}
    if SHOW_ALL_IRQ_MODE == 2:
        for key in IRQS_LIST.keys():
            if key in input_list:
                finally_list.update({key: IRQS_LIST[key]})
    else:
        finally_list = IRQS_LIST

    return finally_list


def draw_irqs(dwg, group, i, irq_final_list):
    """This function is used to draw irq information to svg"""

    tmp_i = i
    irq_name = 'IRQ'
    irq_name_begin = 10 + NAME_WIDTH
    irq_id_begin = irq_name_begin + TGID_WIDTH
    irq_id_end = irq_id_begin + PID_WIDTH + PRIO_WIDTH + \
        POLICY_WIDTH + CPU_RATIO_WIDTH * CPU_NR

    i = i + 1
    draw_irq_flag = 0
    irqs_finally_list = get_finally_irqs_list(irq_final_list)
    if irqs_finally_list is None:
        return

    j = 0
    for key in irqs_finally_list.keys():

        irqs = irqs_finally_list[key]

        x_two_size = BASE_HEIGHT + IOWAIT_HEIGHT + i * ROW_HEIGHT
        line_start = x_two_size - 15
        line_end = line_start + ROW_HEIGHT

        table_group = group.add(dwg.g())

        is_draw_flag = 0
        background_drawn = False
        for irq in irqs:
            end = (float(irq.time) / TIME_UNIT)
            time = float((irq.data >> 16) & 0xffff) / IRQ_DATA_UNIT
            start = end - time
            if start <= START_TIME:
                if end > START_TIME:
                    start = START_TIME
                else:
                    continue
            if end >= END_TIME:
                if start < END_TIME:
                    end = END_TIME
                else:
                    break

            if background_drawn is False and (i % 2) != 0:
                table_group.add(dwg.rect(
                    class_='Sleeping', insert=(8, line_start),
                    size=(SVG_WIDTH, ROW_HEIGHT)))
                background_drawn = True

            # draw irq as minimum value(1us), if time is 0.
            if time == 0:
                end = start + IRQ_MIN_VAL_MS
            cpuid_class = 'CPU%d' % irq.cpuid
            table_group.add(dwg.rect(class_=cpuid_class, insert=((
                start - START_TIME) * RATE + BASE_WIDTH, x_two_size - 15),
                size=((end - start) * RATE, ROW_HEIGHT)))
            is_draw_flag += 1
            if SHOW_TIME_INFO_FLAG == 1:
                table_group.add(dwg.text(
                    '%d' % (time * IRQ_DATA_UNIT),
                    dx=[(start - START_TIME) * RATE + BASE_WIDTH],
                    dy=[x_two_size], class_='ProcessLabel'))
        if is_draw_flag > 0:
            draw_irq_flag = 1
            j += 1
            x_two_size = BASE_HEIGHT + IOWAIT_HEIGHT + i * ROW_HEIGHT
            table_group.add(dwg.text(
                irq_name, dx=[10], dy=[x_two_size], class_='ProcessLabel'))
            table_group.add(dwg.line(
                class_='Bold', start=(irq_name_begin, line_start),
                end=(irq_name_begin, line_end)))
            if key == SNSC_LCTRACER_IPI_IRQ:
                irq_desc = IRQ_DESC_DICT.get('IPI')
                table_group.add(dwg.text(
                    'IPI', dx=[2 + irq_name_begin], dy=[x_two_size],
                    class_='ProcessLabel'))
            elif key == SNSC_LCTRACER_LOC_IRQ:
                irq_desc = IRQ_DESC_DICT.get('LOC')
                table_group.add(dwg.text(
                    'LOC', dx=[2 + irq_name_begin], dy=[x_two_size],
                    class_='ProcessLabel'))
            else:
                irq_desc = IRQ_DESC_DICT.get('%d' % key)
                table_group.add(dwg.text(
                    '%d' % key, dx=[2 + irq_name_begin], dy=[x_two_size],
                    class_='ProcessLabel'))
            table_group.add(dwg.line(
                class_='Bold', start=(irq_id_begin, line_start),
                end=(irq_id_begin, line_end)))
            table_group.add(dwg.text(
                irq_desc, dx=[8 + irq_id_begin], dy=[x_two_size],
                class_='ProcessLabel'))
            table_group.add(dwg.line(
                class_='Bold', start=(irq_id_end, line_start),
                end=(irq_id_end, line_end)))
            table_group.add(dwg.rect(
                class_='Process', insert=(8, line_start),
                size=(SVG_WIDTH, ROW_HEIGHT)))
        else:
            i -= 1
        i += 1
    if draw_irq_flag == 1:
        x_two_size = BASE_HEIGHT + IOWAIT_HEIGHT + tmp_i * ROW_HEIGHT
        group.add(dwg.text(
            'IRQ', dx=[10], dy=[x_two_size], class_='LegendTextBold'))
        group.add(dwg.line(
            class_='Bold', start=(irq_name_begin, x_two_size - 15),
            end=(irq_name_begin, x_two_size - 15 + ROW_HEIGHT)))
        group.add(dwg.text(
            'irq_id', dx=[12 + NAME_WIDTH], dy=[x_two_size],
            class_='LegendTextBold'))
        group.add(dwg.line(
            class_='Bold', start=(irq_id_begin, x_two_size - 15),
            end=(irq_id_begin, x_two_size - 15 + ROW_HEIGHT)))
        group.add(dwg.text(
            'IRQ description', dx=[8 + irq_id_begin], dy=[x_two_size],
            class_='LegendTextBold'))
        group.add(dwg.rect(
            class_='Process', insert=(8, x_two_size - 15),
            size=(SVG_WIDTH, ROW_HEIGHT)))
        group.add(dwg.line(
            class_='Bold', start=(irq_id_end, x_two_size - 15),
            end=(irq_id_end, x_two_size - 15 + ROW_HEIGHT)))

    return i


def draw_horizontal(dwg, table, final_list):
    """This function is used to draw svg with horizontal"""

    i = 1
    j = 0
    group = table.add(dwg.g(
        class_='ProcessStatus', transform='translate(0, 16)'))
    if len(final_list) > 0:
        i = draw_thread(dwg, group, i, final_list)
    x_size = BASE_HEIGHT + IOWAIT_HEIGHT + i * ROW_HEIGHT - 15
    if SHOW_ALL_IRQ_MODE > 0 or SHOW_TID_IRQ_INFO_FLAG == 0:
        if i == 1:
            j = draw_irqs(dwg, group, 0, IRQ_INPUT_LIST)
        else:
            j = draw_irqs(dwg, group, i + 1, IRQ_INPUT_LIST)
    if i != 1 and j != i + 2:
        group.add(dwg.rect(
            class_='THRD1', insert=(8, x_size), size=(SVG_WIDTH, ROW_HEIGHT)))

    return


def get_tid_through_tgid(thread_dict, tgid_list):
    """This function is used to get tig through tgid"""

    tid_list = []
    if len(tgid_list) == 0:
        return tid_list

    # get the thread id from lctracer.dat
    for key in thread_dict.keys():
        data = thread_dict.get(key)
        for i in range(0, len(data)):
            tid = get_tid_by_key(key)
            if data[i].tgid in tgid_list and tid == data[i].ppid:
                tid_list.append(tid)
                break

    if len(tid_list) == 0:
        print 'Warning: input tgid does not exist!'
    return tid_list


def draw_in_2_lines(dwg, table, final_thread_list):
    """This function is used to draw in two line in SVG"""

    i = 0
    clour = 'THRD1'
    thread_clour_flag = 0

    tid_x = 8
    tid_start_height = IOWAIT_HEIGHT + BASE_HEIGHT
    irq_start_height = IOWAIT_HEIGHT + BASE_HEIGHT + ROW_HEIGHT
    table.add(dwg.text(
        'Thread', class_='ProcessLabel',
        dx=[tid_x + 2], dy=[tid_start_height + 15]))
    table.add(dwg.text(
        'IRQ', class_='ProcessLabel',
        dx=[tid_x + 2], dy=[irq_start_height + 15]))
    table.add(dwg.line(
        class_='Bold', start=(NAME_WIDTH, tid_start_height),
        end=(NAME_WIDTH, irq_start_height + ROW_HEIGHT)))

    # Draw thread thread
    list_len = len(final_thread_list)
    j = 0
    for j in range(j, list_len):
        key = final_thread_list[j][0]
        tid = get_tid_by_key(key)
        thread = final_thread_list[j][1]
        thread_len = len(thread)
        i = 0
        for i in range(i, thread_len):
            if i + 1 == thread_len:
                break

            data = thread[i]
            next_data = thread[i + 1]
            if data.npid != tid:
                continue
            if tid != next_data.ppid:
                continue

            start = float(data.time) / TIME_UNIT
            end = float(next_data.time) / TIME_UNIT
            if start <= START_TIME:
                if end > START_TIME:
                    start = START_TIME
                else:
                    continue
            if end >= END_TIME:
                if start < END_TIME:
                    end = END_TIME
                else:
                    break

            table_group = table.add(dwg.g(style='fill:none'))
            tid_running_time = 0
            x_size = BASE_WIDTH
            y_size = 0
            if (SHOW_ALL_TGID_MODE > 0 or SHOW_ALL_TID_MODE > 0)\
                    or SHOW_TID_IRQ_INFO_FLAG == 0:
                y_size = tid_start_height
                tid_running_time = (end - start) * RATE
                x_size = (start - START_TIME) * RATE + BASE_WIDTH
                if thread_clour_flag == 1:
                    clour = 'THRD'
                    thread_clour_flag = 0
                else:
                    clour = 'THRD1'
                    thread_clour_flag = 1
                table_group.add(dwg.rect(
                    class_=clour, insert=(x_size, y_size),
                    size=(tid_running_time, ROW_HEIGHT)))
            irq_running_time = 0
            if thread[i].mode == USER_INFO:
                start = float(thread[i].time) / TIME_UNIT
                x_size = (start - START_TIME) * RATE + BASE_WIDTH
                y_size = tid_start_height
                table_group.add(dwg.rect(
                    class_='USER', insert=(x_size, y_size),
                    size=(1, ROW_HEIGHT)))
                if SHOW_TIME_INFO_FLAG == 1:
                    table_group.add(dwg.text(
                        '%2.6f' % start, class_='AxisLabel',
                        insert=(x_size, y_size + 15)))
                if SHOW_FUNC_INFO_FLAG == 1:
                    thread_function = '%s+%x(%x)' % (
                        data.message, data.data, data.offset)
                    table_group.add(dwg.text(
                        thread_function, class_='AxisLabel',
                        insert=(x_size, y_size + 10), dx='1', dy='3'))

            # Show the time information on chart
            if SHOW_TIME_INFO_FLAG == 1:
                if data.npid != data.ppid:
                    table_group.add(dwg.text(
                        '%2.6f' % start, class_='AxisLabel',
                        insert=(x_size, y_size + 15)))
                    table_group.add(dwg.text(
                        '%2.6f' % end, class_='AxisLabel',
                        insert=(x_size + irq_running_time, y_size + 15)))
            # Show the function information on chart
            if SHOW_FUNC_INFO_FLAG == 1 and data.mode == THREAD_INFO:
                if data.npid != data.ppid:
                    if 0 != data.state:
                        thread_function = '%s+%x(%x)' % (
                            data.message,
                            data.data,
                            data.offset)
                        table_group.add(dwg.text(
                            thread_function, class_='AxisLabel',
                            insert=(x_size, y_size + 10), dx='1', dy='3'))
    if SHOW_TID_IRQ_INFO_FLAG == 0 or SHOW_ALL_IRQ_MODE > 0:
        draw_in_2_line_irq(dwg, table)

    table.add(dwg.rect(
        class_='Process', insert=(tid_x, tid_start_height),
        size=(SVG_WIDTH - 8, ROW_HEIGHT)))
    table.add(dwg.rect(
        class_='Process', insert=(tid_x, irq_start_height),
        size=(SVG_WIDTH - 8, ROW_HEIGHT)))
    return


def draw_in_2_line_irq(dwg, table_group):
    """This function is used to draw in two line of irq information to SVG"""

    irq_clour_flag = 0
    irqs_finally_list = get_finally_irqs_list(IRQ_INPUT_LIST)
    if IRQS_LIST is None:
        return

    for key in irqs_finally_list.keys():
        irq = irqs_finally_list[key]
        irq_len = len(irq)
        i = 0
        x_size = 0
        y_size = 0
        for i in range(i, irq_len):
            if i + 1 == irq_len:
                break
            data = irq[i]
            end = float(data.time) / TIME_UNIT
            irq_running_time = float(data.data >> 16 & 0xffff) / IRQ_DATA_UNIT
            start = end - irq_running_time
            if start <= START_TIME:
                if end > START_TIME:
                    start = START_TIME
                else:
                    continue
            if end >= END_TIME:
                if start < END_TIME:
                    end = END_TIME
                else:
                    break
            if irq_clour_flag == 1:
                clour = 'IRQ'
                irq_clour_flag = 0
            else:
                clour = 'IRQ1'
                irq_clour_flag = 1
            x_size = (start - START_TIME) * RATE + BASE_WIDTH
            y_size = IOWAIT_HEIGHT + BASE_HEIGHT + ROW_HEIGHT
            table_group.add(dwg.rect(
                class_=clour, insert=(x_size, y_size),
                size=(irq_running_time, ROW_HEIGHT)))
            if SHOW_TIME_INFO_FLAG == 1:
                table_group.add(dwg.text(
                    '%d' % (irq_running_time * IRQ_DATA_UNIT),
                    class_='AxisLabel', insert=(x_size, y_size + 15)))
    return


def select_thread_by_input_param(thread_dict):
    """This function is used to select thread by input parameter"""

    input_list = []
    if (SHOW_ALL_TGID_MODE == 1 or SHOW_ALL_TID_MODE == 1) or\
            SHOW_TID_IRQ_INFO_FLAG == 0:
        return
    else:
        if SHOW_ALL_TGID_MODE == 2:
            input_list += get_tid_through_tgid(thread_dict, TGID_INPUT_LIST)
        if SHOW_ALL_TID_MODE == 2:
            input_list += TID_INPUT_LIST

    for key in thread_dict.keys():
        tid = get_tid_by_key(key)
        if tid not in input_list:
            thread_dict.pop(key)


def update_table_to_thread(thread_dict):
    """This function is used to update thread table information to
    thread_dict, which include priority, tgid, function and so on"""

    for key in thread_dict.keys():
        tid = get_tid_by_key(key)
        data = thread_dict[key]
        for i in range(0, len(data)):
            if tid in THD_TAB_PRIO_LIST.keys() and \
                tid == data[i].ppid:
                data[i].tname = THD_TAB_TNAME_LIST[tid]
                data[i].prio = int(THD_TAB_PRIO_LIST[tid][1]) & 0xff
                data[i].tgid = THD_TAB_TGID_LIST[tid]

                ppolicy = THD_TAB_PRIO_LIST[tid][0]
                if ppolicy in THREAD_POLICY_TYPE_DICT.values():
                    policy = dict((v, k) for k, v in \
                        THREAD_POLICY_TYPE_DICT.items()).get(ppolicy)
                else:
                    policy = THREAD_POLICY_TYPE_UNKNOWN
                    if tid not in POLICY_ERR_DICT.keys():
                        POLICY_ERR_DICT.update({tid: ppolicy})
                        print 'Warning: thread %d has unknown policy "%s" ' \
                            'in thread table' % (tid, ppolicy)

                data[i].policy = policy

    return thread_dict


def calc_cpu_ratio(thread_dict):
    """This function is used to calculate cpu ratio"""

    total_time = END_TIME - START_TIME
    for key in thread_dict.keys():
        j = 1
        per_run_time = [0, 0, 0, 0, 0, 0, 0, 0]
        run_time = 0
        data = thread_dict.get(key)
        for j in range(j, len(data)):
            tid = get_tid_by_key(key)
            if tid != data[j - 1].npid or tid != data[j].ppid:
                continue

            start = max(float(data[j - 1].time) / TIME_UNIT, \
                START_TIME)
            end = min(float(data[j].time) / TIME_UNIT, END_TIME)

            if end == 0 or end < start:
                continue

            cpuid = data[j - 1].cpuid
            per_run_time[cpuid] += end - start
            run_time += end - start

        for cpuid in range(0, CPU_NR):
            data[-1].per_cpu_ratio[cpuid] = \
                float(per_run_time[cpuid]) / total_time * 100

        data[-1].cpu_ratio = float(run_time) / total_time * 100

    return


def get_tid_by_key(key):
    if key >= SWAPPER_KEY_BASE:
        return 0
    else:
        return key


def get_thread_info():
    """This function is used to get thread information"""

    # Separate swapper process for each CPU
    data = THREADS_LIST.get(0)
    for cpu_id in range(0, CPU_NR):
        new_data = copy.deepcopy(data)
        THREADS_LIST.update({SWAPPER_KEY_BASE + cpu_id: new_data})
        i = 0
        while i < len(new_data):
            if new_data[i].cpuid != cpu_id:
                new_data.pop(i)
                continue
            i += 1
    THREADS_LIST.pop(0)

    # delete invalid data and thread
    for key in THREADS_LIST.keys():
        data = THREADS_LIST.get(key)
        while len(data) > 1:
            tid = get_tid_by_key(key)
            if tid != data[0].npid or tid != data[1].ppid:
                data.pop(0)
            elif tid != data[-2].npid or tid != data[-1].ppid:
                data.pop(-1)
            else:
                break
        if len(data) <= 1:
            THREADS_LIST.pop(key)

    select_thread_by_input_param(THREADS_LIST)

    thread_dict = update_table_to_thread(THREADS_LIST)

    calc_cpu_ratio(thread_dict)

    thread_sort_func = {
        0: thread_sort_by_time,
        1: thread_sort_by_tgid,
        2: thread_sort_by_tid,
        3: thread_sort_by_prio,
        4: thread_sort_by_policy,
        5: thread_sort_by_ratio,
    }[THREAD_SORT_TYPE]

    return thread_sort_func(thread_dict.items())


def thread_sort_by_time(threads):
    if THREAD_SORT_ORDER:
        return sorted(threads, lambda x, y: \
            cmp(x[1][0].time, y[1][0].time), reverse=True)
    else:
        return sorted(threads, lambda x, y: \
            cmp(x[1][0].time, y[1][0].time))


def thread_sort_by_tgid(threads):
    if THREAD_SORT_ORDER:
        return sorted(threads, lambda x, y: \
            cmp(x[1][-1].tgid, y[1][-1].tgid), reverse=True)
    else:
        return sorted(threads, lambda x, y: \
            cmp(x[1][-1].tgid, y[1][-1].tgid))


def thread_sort_by_tid(threads):
    if THREAD_SORT_ORDER:
        return sorted(threads, lambda x, y: \
            cmp(x[1][-1].ppid, y[1][-1].ppid), reverse=True)
    else:
        return sorted(threads, lambda x, y: \
            cmp(x[1][-1].ppid, y[1][-1].ppid))


def thread_sort_by_prio(threads):
    if THREAD_SORT_ORDER:
        return sorted(threads, lambda x, y: \
            cmp(x[1][-1].prio, y[1][-1].prio), reverse=True)
    else:
        return sorted(threads, lambda x, y: \
            cmp(x[1][-1].prio, y[1][-1].prio))


def thread_sort_by_policy(threads):
    if THREAD_SORT_ORDER:
        return sorted(threads, lambda x, y: \
            cmp(x[1][-1].policy, y[1][-1].policy), reverse=True)
    else:
        return sorted(threads, lambda x, y: \
            cmp(x[1][-1].policy, y[1][-1].policy))


def thread_sort_by_ratio(threads):
    if THREAD_SORT_ORDER:
        return sorted(threads, lambda x, y: \
            cmp(x[1][-1].cpu_ratio, y[1][-1].cpu_ratio), reverse=True)
    else:
        return sorted(threads, lambda x, y: \
            cmp(x[1][-1].cpu_ratio, y[1][-1].cpu_ratio))


def basic_shapes(name, css):
    """This function is used to draw basic shapes"""

    dwg = svgwrite.Drawing(filename=name, debug=True, size=(
        SVG_WIDTH + BASE_WIDTH + SCALE, SVG_HEIGHT + IOWAIT_HEIGHT + SCALE))
    dwg.defs.add(dwg.style(css))
    dwg.add(dwg.rect(class_='Back', rx=None, ry=None, size=(
        SVG_WIDTH + BASE_WIDTH + SCALE, SVG_HEIGHT + IOWAIT_HEIGHT + SCALE)))
    table = dwg.add(dwg.g(transform="translate(10, 50)", style="fill: none"))
    draw_header(dwg, table)
    draw_io_wait(dwg, table)
    draw_coordinate(dwg, table)

    final_thread_list = []
    if SHOW_ALL_TID_MODE != 0 or SHOW_ALL_TGID_MODE != 0 \
            or SHOW_TID_IRQ_INFO_FLAG == 0:
        final_thread_list = get_thread_info()
    if OUTPUT_FILE_FLAG == 1:
        draw_horizontal(dwg, table, final_thread_list)
    else:
        draw_in_2_lines(dwg, table, final_thread_list)

    dwg.save()


def output_txt_file(output_file):
    """This function uses to output format txt"""

    i = 0
    data_buff = ''
    file_obj = open(output_file, 'w')

    input_list = []
    global SHOW_ALL_TID_MODE
    global SHOW_ALL_TGID_MODE
    global SHOW_ALL_IRQ_MODE
    global CPU_NR
    global SNSC_LCTRACER_IPI_IRQ
    global SNSC_LCTRACER_LOC_IRQ

    header_title = 'LCTracer chart(V%s.%s):%s\n' %\
        (VERSION[0], VERSION[1], TITLE_NAME)
    header_uname = 'System uname: %s\n' % SYSTEM_UNAME
    header_cpu = 'CPU:         %s' % SYSTEM_CPU
    header_cpu_nr = 'CPU number:   %d\n' % CPU_NR
    header_lctracer = 'LCTracer parameters state:\n'
    header_buffer_size = 'Buffer size:             %s' % BUFFER_SIZE
    header_threshold = 'Threshold:               %s' % THRESHOLD
    header_autostart = 'Autostart:               %s' % AUTOSTART
    header_filesystem = 'Store to filesystem:     %s' % STORE_TO_FILESYSTEM
    header_interval = 'IO wait sample interval: %s' % IOWAIT_SAMPLE_INTERVAL
    header_save_path = 'Saving path:             %s' % SAVE_PATH
    header_buffer_address = 'Buffer address:          %s\n\n' % BUFFER_ADDRESS

    header_info = header_title + header_uname + header_cpu + header_cpu_nr +\
        header_lctracer + header_buffer_size + header_threshold +\
        header_autostart + header_filesystem + header_interval +\
        header_save_path + header_buffer_address
    file_obj.write(header_info)

    if SHOW_TID_IRQ_INFO_FLAG == 1:
        if SHOW_ALL_TGID_MODE == 2:
            input_list += get_tid_through_tgid(THREADS_LIST, TGID_INPUT_LIST)
        if SHOW_ALL_TID_MODE == 2:
            input_list = input_list + TID_INPUT_LIST
    else:
        SHOW_ALL_IRQ_MODE = 3
        SHOW_ALL_TGID_MODE = 3
        SHOW_ALL_TID_MODE = 3

    while i < len(BINARY_DATA):
        data = BINARY_DATA[i]
        if data.mode == IRQ_INFO and SHOW_ALL_IRQ_MODE == 2 and\
                (data.data & 0xffff) not in IRQ_INPUT_LIST:
            BINARY_DATA.pop(i)
            continue
        if data.mode != IRQ_INFO and\
                (SHOW_ALL_TGID_MODE == 2 or SHOW_ALL_TID_MODE == 2):
            if (data.npid not in input_list and
                    data.ppid not in input_list):
                BINARY_DATA.pop(i)
                continue
        i += 1

    thread_dict = update_table_to_thread(THREADS_LIST)

    for i in range(0, len(BINARY_DATA)):
        data = BINARY_DATA[i]
        time = float(data.time) / TIME_UNIT
        if time < START_TIME:
            continue
        if time > END_TIME:
            break
        line = ''
        time = '%0.6f' % (math.floor((float(data.time) / 1000)) / TIME_UNIT)
        if SHOW_ALL_IRQ_MODE > 0 and data.mode == IRQ_INFO:
            line = '[ %13s ] ' % time
            line += '-cpu%d ' % data.cpuid
            line += '-int '
            irq_number = data.data & 0xffff
            executing_time = ((data.data >> 16) & 0xffff)
            if irq_number == SNSC_LCTRACER_IPI_IRQ:
                line += 'irq:IPI '
                line += 'exe:%d ' % (executing_time)
                line += 'desc:%s\n' % IRQ_DESC_DICT.get('IPI')
            elif irq_number == SNSC_LCTRACER_LOC_IRQ:
                line += 'irq:LOC '
                line += 'exe:%d ' % (executing_time)
                line += 'desc:%s\n' % IRQ_DESC_DICT.get('LOC')
            else:
                line += 'irq:%d ' % irq_number
                line += 'exe:%d ' % (executing_time)
                line += 'desc:%s\n' % IRQ_DESC_DICT.get('%d' % irq_number)
            data_buff += line
        elif (SHOW_ALL_TID_MODE > 0 or SHOW_ALL_TGID_MODE > 0) and\
                (data.ppid != data.npid):
            ppid = data.ppid
            line = '[ %13s ] ' % time
            line += '-cpu%d ' % data.cpuid
            line += '-ctx '
            tgid = data.tgid
            if data.state in TASK_STATE_BITMASK.keys():
                task_state = TASK_STATE_BITMASK[data.state]
            else:
                task_state = 'UNKNOWN_TYPE(0x%x)' % data.state
            line += 'prev:%d:%d -> next:%d pstate:%s '\
                    % (ppid, tgid, data.npid, task_state)
            if 0 == data.state:
                line += 'pwchan:0(0) '
            else:
                line += 'pwchan:%s+%x(%x) '\
                        % (data.message,
                        data.data, data.offset)
            line += 'ptask:%s ' % data.tname
            pprio = str(data.prio)
            line += 'pprio:%s ' % pprio
            if data.policy in THREAD_POLICY_TYPE_DICT.keys():
                ppolicy = THREAD_POLICY_TYPE_DICT.get(data.policy)
            else:
                ppolicy = '"%d"' % data.policy
            line += 'ppolicy:%s\n' % ppolicy

            data_buff += line
        elif data.mode == USER_INFO:
            line = '[ %13s ] ' % time
            line += '-cpu%d ' % data.cpuid
            line += '-usr current:%d log:' % data.npid
            line += '%s\n' % data.message
            data_buff += line
    file_obj.write(data_buff)
    file_obj.close()

    return


def get_thread_table(thread_table_name):
    """This function get tgid, function name, priority through thread table"""

    tname_index = 2
    tgid_index = 4
    pid_index = 5
    prio_index = 8

    if not os.path.isfile(thread_table_name):
        return

    thread_table_fd = open(thread_table_name, 'r')

    for line in thread_table_fd.readlines():
        data_list = line.split('|')
        if len(data_list) > 1:
            THD_TAB_TGID_LIST.update(
                {int(data_list[pid_index]): int(data_list[tgid_index])})
            THD_TAB_TNAME_LIST.update({int(
                data_list[pid_index]): str(data_list[tname_index].strip())})
            THD_TAB_PRIO_LIST.update({int(
                data_list[pid_index]):
                data_list[prio_index].split('/')[0].split()})
    thread_table_fd.close()

    return


if __name__ == "__main__":

    ARG_VALUE = sys.argv
    ARGC = len(sys.argv)
    parse_parameter(ARGC, ARG_VALUE)

    OUTPUT_FILE = ARG_VALUE[ARGC - 1]

    parse_log(ARG_VALUE[ARGC - 2])

    # If the input is *.svg now output file is *.svg.svg
    if OUTPUT_FILE_FLAG == 1:
        OUTPUT_FILE += '.svg'
        basic_shapes(OUTPUT_FILE, CSS)
    elif OUTPUT_FILE_FLAG == 2:
        BASE_WIDTH = NAME_WIDTH
        OUTPUT_FILE += '.SVG'
        basic_shapes(OUTPUT_FILE, CSS)
    else:
        OUTPUT_FILE += '.txt'
        output_txt_file(OUTPUT_FILE)

    # print thread info whose string info is non-ASCII
    if len(NON_ASCII_ERR_TNAME_DICT) > 0:
        print 'Warning: below thread name is Non-ASCII!'
        for key in NON_ASCII_ERR_TNAME_DICT.keys():
            print '\tthread id:%d,\tthread name:%s' % (
                key, NON_ASCII_ERR_TNAME_DICT[key])
    if len(NON_ASCII_ERR_USER_DICT) > 0:
        print 'Warning: below user log is Non-ASCII!'
        for key in NON_ASCII_ERR_USER_DICT.keys():
            print '\tthread id:%d,\tuser log:%s' % (
                key, NON_ASCII_ERR_USER_DICT[key])
    if len(NON_ASCII_ERR_WCHAN_DICT) > 0:
        print 'Warning: below wchan info is Non-ASCII!'
        for key in NON_ASCII_ERR_WCHAN_DICT.keys():
            print '\tthread id:%d,\twchan info:%s' % (
                key, NON_ASCII_ERR_WCHAN_DICT[key])
