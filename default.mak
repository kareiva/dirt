# Default makefile.

ZIP_CONTENTS	= file_id.diz *.txt Makefile *.mak *.mk *.nsi *.cpp *.h *.rc \
		  res/* inc/* ref/* vc/*.sln vc/*.vcproj
ZIP_DIR		= dirt-1.0.0a28

default:
	@echo 'What (msvc, unix, mingw, clean, install)?'

msvc:
	@nmake -nologo -f msvc.mak dirt-msvc

msvc-debug:
	@nmake -nologo -f msvc.mak dirt-msvc-debug

msvc-x64:
	@nmake -nologo -f msvc.mak dirt-msvc-x64

msvc-x64-debug:
	@nmake -nologo -f msvc.mak dirt-msvc-x64-debug

unix:
	@$(MAKE) --no-print-directory -f unix.mak dirt-unix

mingw:
	@mingw32-make --no-print-directory -f mingw.mak dirt-mingw

clean:
	rm -f --recursive temp/ debug/ $(ZIP_DIR)
	rm -f *.zip *.tar.gz *.exe *.ini *.key *.o *.obj *.err *~ *.res \
	*.manifest dirt dirtirc bin2c inc.c inc.cpp inc.h *.dp *.d \
	*.aps *.ilk *.pdb *.idb *.dep *.suo *.ncb *.user *.cod *.old \
	BuildLog.htm vc/*.user vc/*.suo vc/*.ncb

install: /usr/local/bin/. unix
	cp -f dirtirc /usr/local/bin/.
	chmod 755 /usr/local/bin/dirtirc

release: zip tgz nsis

zip: $(ZIP_DIR)-source.zip

tgz: $(ZIP_DIR)-source.tar.gz

nsis: upgrade.exe $(ZIP_DIR)-install.exe

$(ZIP_DIR)-source.zip: $(ZIP_CONTENTS)
	rm -f $@
	rm -rf $(ZIP_DIR)
	mkdir $(ZIP_DIR)
	cp --parents -rf -t $(ZIP_DIR) $(ZIP_CONTENTS)
	rm -f $(ZIP_DIR)/inc.c $(ZIP_DIR)/inc.cpp $(ZIP_DIR)/inc.h
	zip -qr $@ $(ZIP_DIR)
	rm -rf $(ZIP_DIR)

$(ZIP_DIR)-source.tar.gz: $(ZIP_CONTENTS)
	rm -f $@
	rm -rf $(ZIP_DIR)
	mkdir $(ZIP_DIR)
	cp --parents -rf -t $(ZIP_DIR) $(ZIP_CONTENTS)
	rm -f $(ZIP_DIR)/inc.c $(ZIP_DIR)/inc.cpp $(ZIP_DIR)/inc.h
	tar -czf $@ $(ZIP_DIR)
	rm -rf $(ZIP_DIR)

upgrade.exe: upgrade.nsi
	rm -f upgrade.exe
	makensis -V2 upgrade.nsi

$(ZIP_DIR)-install.exe: install.exe
	cp -f install.exe $(ZIP_DIR)-install.exe

install.exe: install.nsi License.txt dirtirc.exe Manual.txt ChangeLog.txt upgrade.exe
	rm -f install.exe
	makensis -V2 install.nsi
