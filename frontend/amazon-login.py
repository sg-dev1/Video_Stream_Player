import sys
import logging
import re
import cookielib
import argparse
import mechanize
from bs4 import BeautifulSoup

from constants import USER_AGENT, AMAZON_BASE_URL, COOKIE_FILE


def LogIn(email, password):
    logger = logging.getLogger("mechanize")
    logger.addHandler(logging.StreamHandler(sys.stdout))
    logger.addHandler(logging.FileHandler("/tmp/log.txt"))
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

    browser.open(AMAZON_BASE_URL + "/gp/aw/si.html")
    browser.select_form(name="signIn")
    browser["email"] = email
    browser["password"] = password
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

    error = soup.find("div", attrs={'id': 'message_error'})
    if error:
        print("Log in error: %s" % error)
        sys.exit(1)
    warning = soup.find('div', attrs={'id': 'message_warning'})
    if warning:
        print("Warning: %s" % warning)
        sys.exit(1)
    auth_error = soup.find('div', attrs={'class': 'a-alert-content'})
    if auth_error:
        print("Authentication error: %s" % auth_error)
        sys.exit(1)

    if 'action=sign-out' in response:
        print("Log in successful")

#        with open("/tmp/out.html", "w") as f:
#            f.write(raw_response)

        # save cookie to file
        cj.save(COOKIE_FILE, ignore_discard=True, ignore_expires=True)
    else:
        print("response=\n%s" % response)
        sys.exit(1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Login to Amazon script.")
    parser.add_argument("email")
    parser.add_argument("password")
    args = parser.parse_args()

    print("Using %s and %s" % (args.email, args.password))

    LogIn(args.email, args.password)
