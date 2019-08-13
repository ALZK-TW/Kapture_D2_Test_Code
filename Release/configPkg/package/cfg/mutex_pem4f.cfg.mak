# invoke SourceDir generated makefile for mutex.pem4f
mutex.pem4f: .libraries,mutex.pem4f
.libraries,mutex.pem4f: package/cfg/mutex_pem4f.xdl
	$(MAKE) -f C:\Users\hukid\workspace_v8\Kapture_D2_Test_Code/src/makefile.libs

clean::
	$(MAKE) -f C:\Users\hukid\workspace_v8\Kapture_D2_Test_Code/src/makefile.libs clean

