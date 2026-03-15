EXTENSION =		pg_fuzzyscore
MODULE_big =	pg_fuzzyscore
DATA =			pg_fuzzyscore--0.1.sql
OBJS = 	normalize.o \
		algorithm.o \
		fuzzyscore.o

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)