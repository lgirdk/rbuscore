rbuscore
---------
rbuscore is for the bus communication to support TR181/TR369 and build on top of rtMessage, as an alternate to D-bus. rtMessage provides pub/sub messaging capabilities using Unix domain socket and/or TCP sockets.

Compilation Steps
-----------------
1. The prerequisite for rbuscore is to have rtmessage; Make sure that rtMessage compiled and installed. Ref: https://code.rdkcentral.com/r/plugins/gitiles/rdk/components/opensource/rtmessage/ for instruction.

2. Create a workspace for rbuscore.

   $ mkdir -p ~/workspace/
   $ cd  ~/workspace

3. Export the prefix variable. rtmessage binaries, libraries and headers should have been installed in this path

   $ export PREFIX=${PWD}

4. Create a src folder where this rbuscore going to be downloaded

   $ mkdir -p src
   $ cd src/
   
5. Clone the source

   $ git clone https://code.rdkcentral.com/r/components/opensource/rbuscore


6. Execute the below commands

   $ cd rbuscore
   $ mkdir build
   $ cmake -DBUILD_FOR_DESKTOP=ON -DCMAKE_INSTALL_PREFIX=$PREFIX ../
   $ make
   $ make install
