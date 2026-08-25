import time
import inspect
import __main__ as main
import logging
from datetime import datetime
import os
import sys
import numpy

test_helper_logger = None

def setup_logging():
    FORMAT = '%(asctime)-15s [%(levelname)-5.5s] %(message)s'
    now = datetime.now()

    global test_helper_logger

    script = os.path.splitext(os.path.basename(main.__file__))[0]
    filename = "{script:s}_{:04}{:02}{:02}_{:02}{:02}{:02}.log".format(now.year, now.month, now.day, now.hour, now.minute, now.second, script=script)

    logFormatter = logging.Formatter(FORMAT)
    logging.basicConfig(level=logging.INFO, format=FORMAT, filename=filename)

    test_helper_logger = logging.getLogger()

    consoleHandler = logging.StreamHandler(sys.stdout)
    consoleHandler.setFormatter(logFormatter)
    test_helper_logger.addHandler(consoleHandler)

    return test_helper_logger

class TestError(Exception):
    def __init__(self, value):
        self.value = value
    def __str__(self):
        return repr(self.value)

def test_timeout(timeout, test, loop_delay = 0.5, reason = None, failure = None):
    start = time.clock()
    if reason and test_helper_logger:
        test_helper_logger.info(reason.format(timeout=timeout))
    while True:
        time.sleep(loop_delay)
        if test():
            return True
        if (time.clock() - start) > timeout:
            if failure and test_helper_logger:
                test_helper_logger.error(failure.format(timeout=timeout))
            raise TestError(inspect.getsource(test))

def test_delay(delay, message):
    if delay<1.0:
        test_helper_logger.info('Delay {d:d}ms - "{message}"'.format(message=message,d=int(delay*1000)))
    else:
        test_helper_logger.info('Delay {d:.2f}s - "{message}"'.format(message=message,d=float(delay)))
    time.sleep(delay)

def test_assert(test):
    if test():
        return True
    else:
        raise TestError(inspect.getsource(test))

class TestStats:
    """
    Simple class to let a test record numerical statistics by name.

    Simply call add("My Stat", 7.2) whenever a statistic is needed, then report
    by using report("My Stat", "Useful statistic #73")
    """
    def __init__(self):
        self.results = dict()

    def strip_out_nan_values(self, raw_data):
        nans = 0
        data = []
        for v in raw_data:
            if not numpy.isnan(v):
                data.append(v)
            else:
                nans = nans + 1
        return nans, numpy.array(data, dtype=numpy.single)

    def add(self, name, value):
        if name in self.results:
            self.results[name].append(value)
        else:
            self.results[name] = [value]

    def report(self, name, title, logger=None, raw=True):
        """
        Report the statistics including the mean, min, max, and std-dev of the data.

        The title can include the name of the data item, by using {stat} in the
        title string. This is useful if want to report all statistics.
        """
        if not logger:
            logger = test_helper_logger
        logger.info(title.format(stat=name))
        raw_data = self.results[name]
        if raw:
            logger.info(raw_data)
        if len(raw_data):
            nan, data = self.strip_out_nan_values(raw_data)
            if nan:
                logger.warning("\tWarning {:d} invalid values removed before stats generated".format(nan))
            results = len(data)
            if results:
                logger.info("\t {:f} <= {:f} <= {:f}".format(numpy.min(data),numpy.mean(data),numpy.max(data)))
                if results > 1:
                    logger.info("\tSdev {:f}".format(numpy.std(data,ddof=1)))
            else:
                logger.warning("\tNo valid data. No statistics.")
        else:
            logger.warning("\tNo data. No statistics.")

    def report_all(self, title, raw=True):
        """
        Report all statistics using the passed title.

        Title can include {stat} for the name of the statistic.
        """
        for statistic in sorted(self.results.keys()):
            self.report(statistic, title, raw=raw)

