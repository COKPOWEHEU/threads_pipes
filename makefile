builddir = build
bindir = bin
progname = $(bindir)/prog
srcdir = src

files := main multiproc

CFLAGS  = -Os -Wall -D_REENTERAN -MD -MP -MT build/$(*F).$(arch).o -MF build/dep/$(@F).mk
CXXFLAGS = $(CFLAGS)

LDFLAGS = -lm

# Выбор архитектуры
#arch := arch_lin32
arch := arch_lin64
#arch := arch_win32
#arch := arch_win64

# Выбор режима
mode := mode_debug
#mode := mode_release



##############################################################################################
##############################################################################################
#  Реализации
##############################################################################################
##############################################################################################

CC = $(COMP_PREFIX)gcc$(COMP_SUFFIX)
CXX= $(COMP_PREFIX)g++$(COMP_SUFFIX)
AR = $(COMP_PREFIX)gcc-ar$(COMP_SUFFIX)
RLIB=$(COMP_PREFIX)gcc-ranlib$(COMP_SUFFIX)

#разделитель слов в одной записи
_divider_ = _I_hope_there_will_not_be_this_string_in_flags_or_file_names!№~._

compiler =	arch_win32$(_divider_)i686-w64-mingw32-$(_divider_)-win32\
		arch_win64$(_divider_)x86_64-w64-mingw32-$(_divider_)-win32\
		
		
spec_cflags =	arch_lin32$(_divider_)-m32\
		
spec_ldflags =	arch_lin32$(_divider_)-m32$(_divider_)-B/usr/lib/gcc/i686-linux-gnu/6$(_divider_) -lpthread\
		arch_lin64$(_divider_)-lpthread\
		arch_win32$(_divider_)-static-libgcc$(_divider_)-static-libstdc++$(_divider_)-lmingw32$(_divider_)-mwindows\
		arch_win64$(_divider_)-static-libgcc$(_divider_)-static-libstdc++$(_divider_)-lmingw32$(_divider_)-mwindows$(_divider_)

res_flags =	mode_debug$(_divider_)-gdwarf-2

prognames =	arch_lin32$(_divider_)32\
		arch_lin64$(_divider_)64\
		arch_win32$(_divider_)32.exe\
		arch_win64$(_divider_)64.exe\

COMP_PREFIX = $(word 2,$(subst $(_divider_), ,$(filter $(arch)$(_divider_)%,$(compiler))))
COMP_SUFFIX = $(word 3,$(subst $(_divider_), ,$(filter $(arch)$(_divider_)%,$(compiler))))

CFLAGS += $(filter-out $(arch),$(subst $(_divider_), ,$(filter $(arch)$(_divider_)%,$(spec_cflags))))
CFLAGS += $(filter-out $(mode),$(subst $(_divider_), ,$(filter $(mode)$(_divider_)%,$(res_flags))))

LDFLAGS := $(filter-out $(arch),$(subst $(_divider_), ,$(filter $(arch)$(_divider_)%,$(spec_ldflags)))) $(LDFLAGS)


resname := $(progname)$(word 2,$(subst $(_divider_), ,$(filter $(arch)$(_divider_)%,$(prognames))))

objects = $(addprefix $(builddir)/,$(addsuffix .$(arch).o,$(files)))

.PHONY: clean cleanall

all:	$(objects)
	mkdir -p $(bindir)
	$(CXX) $(objects) $(LDFLAGS) -o $(resname)
	
remake: clean all
clean:
	rm -f $(objects)
	rm -f $(resname)
	rm -rf $(builddir)/dep
release:	allarch
	rm -rf $(builddir)
	
cleanall:
	rm -rf $(builddir)
	rm -f $(progname)*
allarch:
	make -r -j 10 arch=arch_lin32
	make -r -j 10 arch=arch_lin64
	make -r -j 10 arch=arch_win32
	make -r -j 10 arch=arch_win64 
	
	
#знаю, что дикий костыль, но автоматизировать не получается
build/%.arch_lin32.o: $(srcdir)/%.c
	mkdir -p $(builddir)
	$(CC) -c $(CFLAGS) $< -o $@
build/%.arch_lin64.o: $(srcdir)/%.c
	mkdir -p $(builddir)
	$(CC) -c $(CFLAGS) $< -o $@
build/%.arch_win32.o: $(srcdir)/%.c
	mkdir -p $(builddir)
	$(CC) -c $(CFLAGS) $< -o $@
build/%.arch_win64.o: $(srcdir)/%.c
	mkdir -p $(builddir)
	$(CC) -c $(CFLAGS) $< -o $@
	
build/%.arch_lin32.o: $(srcdir)/%.cpp
	mkdir -p $(builddir)
	$(CXX) -c $(CXXFLAGS) $< -o $@
build/%.arch_lin64.o: $(srcdir)/%.cpp
	mkdir -p $(builddir)
	$(CXX) -c $(CXXFLAGS) $< -o $@
build/%.arch_win32.o: $(srcdir)/%.cpp
	mkdir -p $(builddir)
	$(CXX) -c $(CXXFLAGS) $< -o $@
build/%.arch_win64.o: $(srcdir)/%.cpp
	mkdir -p $(builddir)
	$(CXX) -c $(CXXFLAGS) $< -o $@
	
-include $(shell mkdir -p $(builddir)/dep) $(wildcard $(builddir)/dep/*)
