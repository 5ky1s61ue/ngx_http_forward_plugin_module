# ngx_http_forward_plugin_module

## Description
  a nginx plugin demo for forwading traffic to third upstream servers(here we called forwarded servers) and react according to forwarded servers's response;  
  If forwarded server response with specific status code, this plugin module will block the request with forwarded server's response;  
  Otherwise, the original request will forward to the original upstream server.  
  Generally, this module can be used in some scenarios like risk control, bot-mitigation or traffic detection;  
  The general point is when we rely on the on-line/real-time detection, we can use this module.  
  It still have many improvements to be implemented, for example define the flag from forward servers(currently when forward servers return 418 status code, it will perform block action) to decide whether block the request or not, set internal redirect url to origin uri in the forward request,etc ...
  
## How To Build
  1.`./configure --prefix=/path/of/nginx --add-dynamic-module=/path/of/mymodules/`  
  2.`make`  
  3.`make install`  

## How To Use
  do some configurations in nginx.conf as follow:  
  
  `main`  
  *load_module  modules/ngx_http_forward_plugin_module.so;*  
  
  `http|server|location`  
  *forward_plugin /inter_redirect; #set the uri that the original request will be forwarded, serveral forward requests can be specified on the same level.  
  forward_plugin_request_body on|off; #indicates whether forward the original request body, default value will be on;*  
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
          proxy_pass http://forwarded_server;
        }
        ...
      }
        ...
    }
   
