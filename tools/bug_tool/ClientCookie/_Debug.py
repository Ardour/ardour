import ClientCookie

def debug(text):
    if ClientCookie.CLIENTCOOKIE_DEBUG: _debug(text)

def _debug(text, *args):
    if args:
        text = text % args
    ClientCookie.DEBUG_STREAM.write(text+"\n")
