.PHONY: default clean count list

CUR_DIR := $(shell pwd)
SOURCES:=$(shell find ./src -type f \( -name *.h -o -name *.c \) )

default:
	cd target;$(MAKE) all;cd ..;

clean:
	cd target;$(MAKE) clean;cd ..;
	@rm -rf .db

list:
	@echo ${SOURCES}
count:
	@wc -l ${SOURCES} 

