
@include@

SUBDIRS = src

.DEFAULT_GOAL = all-dist

all-dist: all
	@echo "installing '$(pkgnam)' to './bin' dir"
	@test -d ./bin || mkdir ./bin
	@mv ./src/$(pkgnam) ./bin

clean-local:
	test -d ./bin && rm -rf ./bin || true

.PHONY: all-dist

