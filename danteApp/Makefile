TOP = ..
include $(TOP)/configure/CONFIG

DIRS += danteSupport
DIRS += danteSrc
danteSrc_DEPEND_DIRS += danteSupport

DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *db*))
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *Db*))
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *op*))
include $(TOP)/configure/RULES_DIRS

