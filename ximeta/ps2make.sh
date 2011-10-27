( 
    export PATH=/usr/local/ps2dev/bin/:/usr/local/ps2dev/iop/bin:/usr/local/ps2dev/ee/bin/:/usr/bin:/bin ; 
    #export PATH=/usr/local/ps2-03-28-2004/bin/:/usr/local/ps2dev/ee/bin/:/usr/bin:/bin ; 

    #export nxp-cpu=iop ; 
    export nxp-cpu=iop ; 
    export nxp-os=ps2dev ; 
    make $*
)
