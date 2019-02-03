import logging


def configureLogging():
    formatStr = "[%(levelname)7s: %(asctime)s] %(message)s"
    logging.basicConfig(format=formatStr, datefmt='%m/%d/%Y %I:%M:%S %p', level=logging.DEBUG)
    logging.getLogger("requests").setLevel(logging.WARNING)
    logging.getLogger("urllib3").setLevel(logging.WARNING)
