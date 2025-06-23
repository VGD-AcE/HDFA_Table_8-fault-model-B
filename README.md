# HDFA_Table_8-fault-model-B

In order to achieve fast reading and writing of Excel files, we use the libxl library in the project. The following is the specific configuration method:

The files uploaded in the repository already contain the include_cpp folder, lib64 folder and libxl.dll file (in the x64/Release folder) necessary to load the libxl library.

This configuration and usage is intended for compiling and running on Windows systems.

******* Visual Studio environment configurations *******

(Visual Studio 2022) Project – Properties:

C/C++ → General → Additional Include Directories: Set the path to the include_cpp folder.

Linker → General → Additional Library Directories: Set the path to the lib64 folder.

Linker → Input → Additional Dependencies: Add libxl.lib.

Finally, copy the libxl.dll file to the path where the .exe file is located (this project uses x64/Release).
