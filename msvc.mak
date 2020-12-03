# Microsoft Visual C++ makefile
# Requires MSVS 2008 (it uses vcbuild to compile)

dirt-msvc:
	@vcbuild /nologo vc\dirt.sln release^|win32

dirt-msvc-debug:
	@vcbuild /nologo vc\dirt.sln debug^|win32

dirt-msvc-x64:
	@vcbuild /nologo vc\dirt.sln release^|x64

dirt-msvc-x64-debug:
	@vcbuild /nologo vc\dirt.sln debug^|x64

include default.mak
