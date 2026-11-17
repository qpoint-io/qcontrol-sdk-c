# C SDK Examples
#
# Build plugins:  make or make build
# Build file/net/exec/http:  make build-file | build-net | build-exec | build-http
# Clean:  make clean
# Dev container:  make dev

FILE_PLUGINS := file-logger access-control byte-counter content-filter text-transform
EXEC_PLUGINS := exec-logger
NET_PLUGINS := net-logger net-transform
NETSTAT_PLUGINS := netstat-observe
HTTP_PLUGINS := http-logger http-rewrite sse-rewrite websocket-rewrite
PLUGINS := $(FILE_PLUGINS) $(EXEC_PLUGINS) $(NET_PLUGINS) $(NETSTAT_PLUGINS) $(HTTP_PLUGINS)
IMAGE_NAME := qcontrol-sdk-c-demo

.PHONY: all build build-file build-exec build-net build-netstat build-http clean docker-build dev

all: build

build:
	@for p in $(PLUGINS); do $(MAKE) -C $$p build; done

build-file:
	@for p in $(FILE_PLUGINS); do $(MAKE) -C $$p build; done

build-exec:
	@for p in $(EXEC_PLUGINS); do $(MAKE) -C $$p build; done

build-net:
	@for p in $(NET_PLUGINS); do $(MAKE) -C $$p build; done

build-netstat:
	@for p in $(NETSTAT_PLUGINS); do $(MAKE) -C $$p build; done

build-http:
	@for p in $(HTTP_PLUGINS); do $(MAKE) -C $$p build; done

clean:
	@for p in $(PLUGINS); do $(MAKE) -C $$p clean; done

docker-build:
	docker build -t $(IMAGE_NAME) -f Dockerfile ..

dev: docker-build
	docker run --rm -it $(IMAGE_NAME)
