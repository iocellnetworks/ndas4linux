( 
    cp -f /tmp/libndas-b$1.zip /home/broadq/src/medio/ximeta/lib/ && \
    ( 
        cd /home/broadq/src/medio/ximeta/lib ; 
        unzip -o libndas-b$1.zip  ; 
        #cp -f libndasD.a libndas.a 
    ); 
)  && make clean _all && cp -f netdisk.irx  /usr/local/bin/GameShark_Media_Player/system/
