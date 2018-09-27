from http.server import *

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header('Content-type', 'text/html')
        self.end_headers()
        f = open('/home/carl/DCP/KDMs/Enc2_FTR-1_F-133_MOS_2K_20180925_IOP_OV_DKDM.xml', 'r')
        self.wfile.write(bytearray(f.read(), 'UTF-8'))
        f.close()

server_address = ('', 8000)
httpd = HTTPServer(('', 8000), Handler)
httpd.serve_forever()
