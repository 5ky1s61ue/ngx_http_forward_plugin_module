# ngx_http_forward_plugin

## Description
  a nginx plugin for forwading traffic to third upstream servers(here we called forwarded servers) and react according to third forwarded servers's response; If forwarded server response with specific status code, this plugin module will block the request with forwarded server's response; Otherwise, the original request will forward to the original upstream server.
  
## How To Build
  1.`./configure --prefix=/path/of/nginx --add-dynamic-module=/path/of/mymodules/`  
  2.`make`  
  3.`make install`  

## How To Use
  do some configuration in nginx.conf as follow:  
  `main`  
  load_module  modules/ngx_http_forward_plugin_module.so;  
  `http|server|location`  
  forward_plugin /inter_redirect; #set the uri that the original request will be forwarded, serveral forward requests can be specified on the same level.  
  forward_plugin_request_body on|off; #indicates whether forward the original request body, default value will be on;  
  <br><br>
  
## Example

  http{  
    ...
    upstream forwarded_server{
      server server_ip:server_port;
      keepalive 1024;
      ...
    }
    ...
    server{
      ...
      location /test {
        forward_plugin /inter_redirect;
        proxy_set_header Host $http_host;
        proxy_pass http://upstream_server;
      }

      location /inter_redirect {
        internal;
        proxy_set_header Host $http_host;
        proxy_set_header X-Original-URI $request_uri;
        proxy_pass forwarded_server;
      }
      ...
    }
      ...
  }
   
