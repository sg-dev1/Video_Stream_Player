import sys
import re
import os
from http.cookiejar import LWPCookieJar
from urllib.parse import urlencode, urlsplit

import mechanize
from bs4 import BeautifulSoup

from constants import USER_AGENT, AMAZON_BASE_URL, COOKIE_FILE

LOGIN_DEBUG = os.environ.get("AMAZON_LOGIN_DEBUG") == "1"


def _debug(message):
    if LOGIN_DEBUG:
        print("[amazon-login-debug] %s" % message, file=sys.stderr)


def _mask_email(email):
    if "@" not in email:
        return "<redacted>"
    name, domain = email.split("@", 1)
    return "%s***@%s" % (name[:1], domain)


def _sign_in_url():
    # Amazon retired /gp/aw/si.html; its current sign-in page requires the
    # OpenID query parameters used by the account-navigation link.
    params = {
        "openid.return_to": AMAZON_BASE_URL + "/?ref_=nav_ya_signin",
        "openid.identity": "http://specs.openid.net/auth/2.0/identifier_select",
        "openid.assoc_handle": "deflex",
        "openid.mode": "checkid_setup",
        "openid.claimed_id": "http://specs.openid.net/auth/2.0/identifier_select",
        "openid.ns": "http://specs.openid.net/auth/2.0",
    }
    return AMAZON_BASE_URL + "/ap/signin?" + urlencode(params)


def LogIn(email, password):
    _debug("account=%s; password supplied=%s; password length=%d (value redacted)" % (
        _mask_email(email), bool(password), len(password)
    ))
    cj = LWPCookieJar()

    browser = mechanize.Browser()
    browser.set_handle_robots(False)
    browser.set_cookiejar(cj)
    browser.set_handle_gzip(True)
    browser.addheaders = [('User-Agent', USER_AGENT)]

    # enable debug
#    browser.set_debug_http(True)
#    browser.set_debug_responses(True)
#    browser.set_debug_redirects(True)

    response = browser.open(_sign_in_url())
    _debug("GET sign-in page: HTTP %s; URL=%s; cookies=%d" % (
        response.code, urlsplit(response.geturl()).path, len(cj)
    ))
    browser.select_form(name="signIn")
    _debug("selected form 'signIn'; fields=%s" % ",".join(
        str(control.name) for control in browser.form.controls
    ))
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
    response = browser.response()
    _debug("submitted sign-in form: HTTP %s; URL=%s; cookies=%d" % (
        response.code, urlsplit(response.geturl()).path, len(cj)
    ))
    raw_response = response.read()
    if isinstance(raw_response, bytes):
        raw_response = raw_response.decode("utf-8", errors="replace")
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
        message = auth_error.get_text(" ", strip=True)
        help_link = auth_error.find("a", href=True)
        _debug("Amazon auth alert: text=%r; help_path=%r" % (
            message[:300], help_link["href"] if help_link else None
        ))
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
    email = sys.stdin.readline().rstrip("\r\n")
    password = sys.stdin.readline().rstrip("\r\n")
    if not email or not password:
        print("Login credentials were not provided.", file=sys.stderr)
        sys.exit(2)
    LogIn(email, password)
