import sys
import logging
import re
import cookielib
import argparse
import mechanize
from bs4 import BeautifulSoup

from constants import USER_AGENT, AMAZON_BASE_URL, COOKIE_FILE


def LogIn(email, password):
    logger = logging.getLogger(***REMOVED***mechanize***REMOVED***)
    logger.addHandler(logging.StreamHandler(sys.stdout))
    logger.addHandler(logging.FileHandler(***REMOVED***/tmp/log.txt***REMOVED***))
    logger.setLevel(logging.DEBUG)

    cj = cookielib.LWPCookieJar()

    browser = mechanize.Browser()
    browser.set_handle_robots(False)
    browser.set_cookiejar(cj)
    browser.set_handle_gzip(True)
    browser.addheaders = [('User-Agent', USER_AGENT)]

    # enable debug
#    browser.set_debug_http(True)
#    browser.set_debug_responses(True)
#    browser.set_debug_redirects(True)

    browser.open(AMAZON_BASE_URL + ***REMOVED***/gp/aw/si.html***REMOVED***)
    browser.select_form(name=***REMOVED***signIn***REMOVED***)
    browser[***REMOVED***email***REMOVED***] = email
    browser[***REMOVED***password***REMOVED***] = password
    browser.addheaders = [('Accept', 'text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8'),
                     ('Accept-Encoding', 'gzip, deflate'),
                     ('Accept-Language', 'de,en-US;q=0.8,en;q=0.6'),
                     ('Cache-Control', 'max-age=0'),
                     ('Connection', 'keep-alive'),
                     ('Content-Type', 'application/x-www-form-urlencoded'),
                     ('Host', AMAZON_BASE_URL.split('//')[1]),
                     ('Origin', AMAZON_BASE_URL),
                     ('User-Agent', USER_AGENT),
                     ('Upgrade-Insecure-Requests', '1')]
    browser.submit()
    raw_response = browser.response().read()
    response = re.sub(r'(?i)(<!doctype \w+).*>', r'\1>', raw_response)
    soup = BeautifulSoup(response, 'html.parser')

    error = soup.find(***REMOVED***div***REMOVED***, attrs={'id': 'message_error'})
    if error:
        print(***REMOVED***Log in error: %s***REMOVED*** % error)
        sys.exit(1)
    warning = soup.find('div', attrs={'id': 'message_warning'})
    if warning:
        print(***REMOVED***Warning: %s***REMOVED*** % warning)
        sys.exit(1)
    auth_error = soup.find('div', attrs={'class': 'a-alert-content'})
    if auth_error:
        print(***REMOVED***Authentication error: %s***REMOVED*** % auth_error)
        sys.exit(1)

    if 'action=sign-out' in response:
        print(***REMOVED***Log in successful***REMOVED***)

#        with open(***REMOVED***/tmp/out.html***REMOVED***, ***REMOVED***w***REMOVED***) as f:
#            f.write(raw_response)

        # save cookie to file
        cj.save(COOKIE_FILE, ignore_discard=True, ignore_expires=True)
    else:
        print(***REMOVED***response=\n%s***REMOVED*** % response)
        sys.exit(1)


if __name__ == ***REMOVED***__main__***REMOVED***:
    parser = argparse.ArgumentParser(description=***REMOVED***Login to Amazon script.***REMOVED***)
    parser.add_argument(***REMOVED***email***REMOVED***)
    parser.add_argument(***REMOVED***password***REMOVED***)
    args = parser.parse_args()

    print(***REMOVED***Using %s and %s***REMOVED*** % (args.email, args.password))

    LogIn(args.email, args.password)
