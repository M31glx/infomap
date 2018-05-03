CXXFLAGS = -Wall -std=c++14
# http://www.network-theory.co.uk/docs/gccintro/gccintro_70.html -msse2 -mfpmath=sse -DDOUBLE
# CXXFLAGS = -Wall -std=c++14 -DPYTHON -Wno-deprecated-register
LDFLAGS =
CXX_CLANG := $(shell $(CXX) --version 2>/dev/null | grep clang)
ifeq "$(findstring debug, $(MAKECMDGOALS))" "debug"
	CXXFLAGS += -O0 -g
else
	ifeq "$(CXX_CLANG)" ""
		CXXFLAGS += -O4
		ifneq "$(findstring noomp, $(MAKECMDGOALS))" "noomp"
			CXXFLAGS += -fopenmp
			LDFLAGS += -fopenmp
		endif
	else
		CXXFLAGS += -O3
	endif
endif

##################################################
# General file dependencies
##################################################

HEADERS := $(shell find src -name "*.h")
SOURCES := $(shell find src -name "*.cpp")
OBJECTS := $(SOURCES:src/%.cpp=build/Infomap/%.o)

##################################################
# Stand-alone C++ targets
##################################################

.PHONY: all noomp test debug

all: Infomap
	@true

Infomap: $(OBJECTS)
	@echo "Linking object files to target $@..."
	$(CXX) $(LDFLAGS) -o $@ $^
	@echo "-- Link finished --"

## Generic compilation rule for object files from cpp files
build/Infomap/%.o : src/%.cpp $(HEADERS) Makefile
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

noomp: Infomap
	@true

debug: clean Infomap
	@true


##################################################
# JavaScript through Emscripten
##################################################

.PHONY: js js-worker

js: build/js/Infomap.js
	@echo "Built $^"

js-worker: build/js/Infomap-worker.js
	@echo "Built $^"

# em++ -O0 -s PROXY_TO_WORKER=1 -s PROXY_TO_WORKER_FILENAME='Infomap.js' -o Infomap.js $^
# em++ -O0 -s PROXY_TO_WORKER=1 -s EXPORT_NAME='Infomap' -s MODULARIZE=1 -o Infomap.js $^
build/js/Infomap-worker.js: $(SOURCES)
	@echo "Compiling Infomap to run in a worker in the browser..."
	@mkdir -p $(dir $@)
	em++ -O0 -s ALLOW_MEMORY_GROWTH=1 --pre-js interfaces/js/pre-worker-module.js -o build/js/Infomap-worker.js $^

build/js/Infomap.js: $(SOURCES)
	@echo "Compiling Infomap for Node.js..."
	@mkdir -p $(dir $@)
	em++ -O0 -o build/js/Infomap.js $^


##################################################
# Static C++ library
##################################################

# Use separate object files to compile with definitions
# NS_INFOMAP: Wrap code in namespace infomap
# AS_LIB: Skip main function
LIB_DIR = build/lib
LIB_HEADERS := $(HEADERS:src/%.h=include/%.h)
LIB_OBJECTS := $(SOURCES:src/%.cpp=build/lib/%.o)

.PHONY: lib

lib: lib/libInfomap.a $(LIB_HEADERS)
	@echo "Wrote static library to lib/ and headers to include/"

lib/libInfomap.a: $(LIB_OBJECTS) Makefile
	@echo "Creating static library..."
	@mkdir -p lib
	ar rcs $@ $^

# Rule for $(LIB_HEADERS)
include/%.h: src/%.h
	@mkdir -p $(dir $@)
	@cp -a $^ $@

# Rule for $(LIB_OBJECTS)
build/lib/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -DNS_INFOMAP -DAS_LIB -c $< -o $@


##################################################
# General SWIG helpers
##################################################

SWIG_FILES := $(shell find interfaces/swig -name "*.i")


##################################################
# Python module
##################################################

PY_BUILD_DIR = build/py
PY2_BUILD_DIR = build/py
PY_HEADERS := $(HEADERS:src/%.h=$(PY_BUILD_DIR)/src/%.h)
PY_SOURCES := $(SOURCES:src/%.cpp=$(PY_BUILD_DIR)/src/%.cpp)
PY2_HEADERS := $(HEADERS:src/%.h=$(PY2_BUILD_DIR)/src/%.h)
PY2_SOURCES := $(SOURCES:src/%.cpp=$(PY2_BUILD_DIR)/src/%.cpp)

.PHONY: python2 python py2-build py-build

# Use python distutils to compile the module
python2: py2-build Makefile
	@cp -a interfaces/python/setup.py $(PY2_BUILD_DIR)/
	cd $(PY2_BUILD_DIR) && CC=$(CXX) python setup.py build_ext --inplace
	@true

python: py-build Makefile
	@cp -a interfaces/python/setup.py $(PY_BUILD_DIR)/
	cd $(PY_BUILD_DIR) && CC=$(CXX) python3 setup.py build_ext --inplace
	@true

# Generate wrapper files from source and interface files
py-build: $(PY_HEADERS) $(PY_SOURCES)
	@mkdir -p $(PY_BUILD_DIR)
	@cp -a $(SWIG_FILES) $(PY_BUILD_DIR)/
	swig -c++ -python -py3 -outdir $(PY_BUILD_DIR) -o $(PY_BUILD_DIR)/infomap_wrap.cpp $(PY_BUILD_DIR)/Infomap.i

py2-build: $(PY2_HEADERS) $(PY2_SOURCES)
	@mkdir -p $(PY2_BUILD_DIR)
	@cp -a $(SWIG_FILES) $(PY2_BUILD_DIR)/
	swig -c++ -python -outdir $(PY2_BUILD_DIR) -o $(PY2_BUILD_DIR)/infomap_wrap.cpp $(PY2_BUILD_DIR)/Infomap.i

# Rule for $(PY_HEADERS) and $(PY_SOURCES)
$(PY_BUILD_DIR)/src/%: src/%
	@mkdir -p $(dir $@)
	@cp -a $^ $@

$(PY2_BUILD_DIR)/src/%: src/%
	@mkdir -p $(dir $@)
	@cp -a $^ $@


##################################################
# R module
##################################################

R_BUILD_DIR = build/R
R_HEADERS := $(HEADERS:src/%.h=$(R_BUILD_DIR)/src/%.h)
R_SOURCES := $(SOURCES:src/%.cpp=$(R_BUILD_DIR)/src/%.cpp)

.PHONY: R R-build

# Use R to compile the module
R: R-build Makefile
	cd $(R_BUILD_DIR) && CXX="$(CXX)" PKG_CPPFLAGS="$(CXXFLAGS) -DAS_LIB" PKG_LIBS="$(LDFLAGS)" R CMD SHLIB infomap_wrap.cpp $(SOURCES)
	@true

# Generate wrapper files from source and interface files
R-build: Makefile $(R_HEADERS) $(R_SOURCES)
	@mkdir -p $(R_BUILD_DIR)
	@cp -a $(SWIG_FILES) $(R_BUILD_DIR)/
	swig -c++ -r -outdir $(R_BUILD_DIR) -o $(R_BUILD_DIR)/infomap_wrap.cpp $(R_BUILD_DIR)/Infomap.i

# Rule for $(R_HEADERS) and $(R_SOURCES)
$(R_BUILD_DIR)/src/%: src/%
	@mkdir -p $(dir $@)
	@cp -a $^ $@


##################################################
# Clean
##################################################

clean:
	$(RM) -r Infomap build lib include
