.PHONY: all
all: clean configure build

.PHONY: build
build:
	cmake --build build

.PHONY: clean
clean:
	rm -rf build

.PHONY: configure
configure:
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

.PHONY: test
test:
	./build/neural_pipeline_tests
