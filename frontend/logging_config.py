import logging


def configureLogging():
    formatStr = ***REMOVED***[%(levelname)7s: %(asctime)s] %(message)s***REMOVED***
    logging.basicConfig(format=formatStr, datefmt='%m/%d/%Y %I:%M:%S %p', level=logging.DEBUG)
    logging.getLogger(***REMOVED***requests***REMOVED***).setLevel(logging.WARNING)
    logging.getLogger(***REMOVED***urllib3***REMOVED***).setLevel(logging.WARNING)
