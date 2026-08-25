import logging


class VaAction(object):
    def __init__(self):
        # Get logger with class name of child class inheriting from this one
        self.log = logging.getLogger('wizard.VaAction.{}'.format(type(self).__name__))
