# Cache configuration to smilate the caches of Toshiba TX4938 
valgrind --tool=cachegrind --I1=32768,4,32 --D1=32768,4,32 $1
