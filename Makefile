#----------------------------------------------------------------------------
# NT219 PQC benchmark — Makefile
# Sắp xếp theo layout của pqcbench (lelegard), thích nghi cho repo này:
#   SRCDIR=apps, EXEC=build/bench_evp, OpenSSL link động + rpath, chạy Linux/Pi.
# Hai khác biệt CÓ CHỦ ĐÍCH so với pqcbench (ghi chú ngay tại chỗ):
#   (1) Bản release áp dụng FULLSPEED (-O3 ...) thay vì -O2 mặc định -> khớp yêu cầu -O3.
#   (2) Thêm -Wl,-rpath khi dùng OSSLROOT -> binary tự tìm libcrypto, khỏi cần LD_LIBRARY_PATH.
#----------------------------------------------------------------------------
default: exec

# SYSTEM = linux hoặc mac ; ARCH = x64 hoặc arm64 (dò tự động theo máy)
SYSTEM := $(subst Linux,linux,$(subst Darwin,mac,$(shell uname -s)))
ARCH   := $(subst amd64,x64,$(subst x86_64,x64,$(subst aarch64,arm64,$(shell uname -m))))

# Thư mục và file của dự án.
SRCDIR   = src
BINDIR   = build
EXEC     = $(BINDIR)/bench_evp
SOURCES := $(wildcard $(SRCDIR)/*.cpp)
OBJECTS := $(patsubst $(SRCDIR)/%.cpp,$(BINDIR)/%.o,$(SOURCES))

# Công cụ và tùy chọn chung.
SHELL      = /usr/bin/env bash --noprofile
# Cảnh báo: -Wall -Wextra; giữ -Werror vì harness build sạch (cảnh báo nào cũng thành lỗi).
CXXFLAGS  += -Werror -Wall -Wextra -Wno-unused-parameter
# Cờ tối ưu "full speed" (như pqcbench). pqcbench định nghĩa nhưng không dùng; ở đây áp cho release.
FULLSPEED  = -O3 -fno-strict-aliasing -funroll-loops -fomit-frame-pointer
# Trên macOS: tự thêm đường dẫn include/lib của Homebrew nếu có.
CPPFLAGS  += -std=c++17 $(if $(findstring mac,$(SYSTEM)),$(addprefix -I,$(wildcard /opt/homebrew/include /usr/local/include)))
LDFLAGS   += $(if $(findstring mac,$(SYSTEM)),$(addprefix -L,$(wildcard /opt/homebrew/lib /usr/local/lib)))
LDLIBS    += -lcrypto -lm

# Đặt DEBUG=1 để build chế độ debug. Release áp dụng FULLSPEED (khác pqcbench dùng -O2).
CXXFLAGS += $(if $(DEBUG),-g -O0,$(FULLSPEED))
LDFLAGS  += $(if $(DEBUG),-g)

# Dùng OpenSSL tự build:
#   git clone https://github.com/openssl/openssl.git
#   ./Configure --prefix=/opt/openssl-3.6.2 --libdir=lib shared ; make ; make install
#   make OSSLROOT=/opt/openssl-3.6.2
# Nhờ rpath nên KHÔNG cần đặt LD_LIBRARY_PATH lúc chạy (khác pqcbench).
OSSLLIB := $(if $(wildcard $(OSSLROOT)/lib64),lib64,lib)
CXXFLAGS += $(if $(OSSLROOT),-I$(OSSLROOT)/include)
LDFLAGS  += $(if $(OSSLROOT),-L$(OSSLROOT)/$(OSSLLIB) -Wl$(comma)-rpath$(comma)$(OSSLROOT)/$(OSSLLIB))
comma := ,

# Các thao tác build.
exec: $(EXEC)
	@true
$(EXEC): $(OBJECTS)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
$(BINDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<
run: $(EXEC)
	$(EXEC) rsa 2048
clean:
	rm -rf build build-* core *.tmp *.log __pycache__
distclean: clean
	rm -rf .openssl analysis_out

# Tự sinh lại dependency ngầm (.d). Bỏ qua khi mục tiêu là clean/distclean/listvars/cxxmacros.
ifneq ($(if $(MAKECMDGOALS),$(filter-out clean distclean listvars cxxmacros,$(MAKECMDGOALS)),true),)
    -include $(patsubst $(SRCDIR)/%.cpp,$(BINDIR)/%.d,$(SOURCES))
endif
$(BINDIR)/%.d: $(SRCDIR)/%.cpp
	@mkdir -p $(BINDIR)
	$(CXX) -MM $(CPPFLAGS) -MT $(BINDIR)/$*.o -MT $@ $< >$@ || rm -f $@

# In ra các biến của make (phục vụ debug).
listvars:
	@true
	$(foreach v, \
	  $(sort $(filter-out .% ^% @% _% *% \%% <% +% ?% BASH% LS_COLORS SSH% VTE% XDG% F_%,$(.VARIABLES))), \
	  $(info $(v) = "$($(v))"))
# In ra các macro C++ định nghĩa sẵn (phục vụ debug).
cxxmacros:
	@$(CPP) $(CXXFLAGS) -x c++ -dM /dev/null | sort

.PHONY: default exec run clean distclean listvars cxxmacros