BIN_NAME := my_project
ifeq ($(OS),Windows_NT)
    PLATFORM_OS := WINDOWS
    BIN_EXT := .exe
else
    PLATFORM_OS := LINUX
    BIN_EXT :=
endif

BIN_NAME:=$(BIN_NAME)$(BIN_EXT)

# Szuka wszystkich plików .proto w katalogu api i głębiej
PROTO_FILES := $(shell find api -name "*.proto")

.PHONY: all clean run build test proto

all: proto test build ## Build and run tests

test: proto ## Run unit tests
	@echo "Running tests..."
	@go test ./...

build: proto
	@echo "Building executable..."
	@go build -v -o $(BIN_NAME) cmd/main.go

run: build
	@echo "Running app..."
	@go run cmd/main.go

## proto: Generuje kod Go oraz gRPC ze wszystkich plików .proto w katalogu api
proto:
	@if [ -z "$(PROTO_FILES)" ]; then \
		echo "No .proto files found in api directory/"; \
	else \
		echo "Generating go files from proto..."; \
		protoc \
			--go_out=. \
			--go_opt=paths=source_relative \
			--go-grpc_out=. \
			--go-grpc_opt=paths=source_relative \
			$(PROTO_FILES); \
		echo "Gotowe!"; \
	fi


clean: ## Remove previous build
	@echo "Remove binary..."
	@rm -f $(BIN_NAME)
	@echo "Remove generated go files..."
	@find api -name "*.pb.go" -delete
