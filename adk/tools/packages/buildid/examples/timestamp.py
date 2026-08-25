# Copyright (c) 2017 - 2021 Qualcomm Technologies International, Ltd.
import sys
import time

try:
    from vm_build.buildid.writer import BuildIdWriter

except ImportError:
    from buildid.writer import BuildIdWriter

class BuildIdGen(object):

    def __init__(self):
        self.__now = time.time()

    def get_id_string(self):
        local_time = time.localtime(self.__now)
        number = self.get_id_number()
        return "QTIL ADK {:s} @{:d}".format(time.strftime('%Y-%m-%d %H:%M:%S', local_time), number)

    def get_id_number(self):
        UINT32_MAX = 4294967295
        return int(self.__now) % UINT32_MAX


if __name__ == "__main__":
    build_id_string_file = sys.argv[1]
    gen = BuildIdGen()
    BuildIdWriter.write(build_id_string_file, gen.get_id_number(), gen.get_id_string())
