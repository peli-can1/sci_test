CC=g++
TARGETDIR=build/
TARGET=$(TARGETDIR)sci_test

CFLAGS= -std=gnu++11

UTILS=utils
APP=app
SERIAL=serial

LIBS= -lpthread -lboost_system -lboost_thread -lboost_date_time -lboost_regex -lboost_serialization -lboost_filesystem

SOURCE = $(UTILS)/Trace.cpp
SOURCE += $(UTILS)/GetOpt.cpp
SOURCE += $(APP)/sci_test.cpp
SOURCE += $(SERIAL)/TimeoutSerialThread.cpp

INCLUDE = -I$(UTILS) -I$(APP) -I$(SERIAL)

VPATH=$(UTILS) $(APP) $(SERIAL)
OBJ=$(join $(addsuffix ../$(TARGETDIR), $(dir $(SOURCE))), $(notdir $(SOURCE:.cpp=.o)))

## Fix dependency destination to be ../.dep relative to the src dir
DEPENDS=$(join $(addsuffix ../.dep/, $(dir $(SOURCE))), $(notdir $(SOURCE:.cpp=.d)))

## Default rule executed
all: $(TARGET)
	@true

## Clean Rule
clean:
	@-rm -f $(TARGET) $(OBJ) $(DEPENDS)


## Rule for making the actual target
$(TARGET): $(OBJ)
	@echo "============="
	@echo "Linking the target $@"
	@echo "============="
	@$(CC) $(CFLAGS) -o $@ $^ $(LIBS)
	@echo -- Link finished --

## Generic compilation rule
%.o : %.cpp
	@mkdir -p $(dir $@)
	@echo "============="
	@echo "Compiling $<"
	@$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@


## Rules for object files from cpp files
## Object file for each file is put in obj directory
## one level up from the actual source directory.
$(TARGETDIR)%.o : %.cpp
	@mkdir -p $(dir $@)
	@echo "============="
	@echo "Compiling $<"
	@$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

# Rule for "other directory"  You will need one per "other" dir
$(UTILS)/../$(TARGETDIR)%.o : %.cpp
	@mkdir -p $(dir $@)
	@echo "============="
	@echo "Compiling $<"
	@$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

# Rule for "other directory"  You will need one per "other" dir
$(SERIAL)/../$(TARGETDIR)%.o : %.cpp
	@mkdir -p $(dir $@)
	@echo "============="
	@echo "Compiling $<"
	@$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

# Rule for "other directory"  You will need one per "other" dir
$(APP)/../$(TARGETDIR)%.o : %.cpp
	@mkdir -p $(dir $@)
	@echo "============="
	@echo "Compiling $<"
	@$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

## Make dependency rules
.dep/%.d: %.cpp
	@mkdir -p $(dir $@)
	@echo "============="
	@echo Building dependencies file for $*.o
	@$(SHELL) -ec '$(CC) -MM $(CFLAGS) $< | sed "s^$*.o^$(TARGETDIR)$*.o^" > $@'

## Dependency rule for "other" directory
$(UTILS)/../.dep/%.d: %.cpp
	@mkdir -p $(dir $@)
	@echo "============="
	@echo Building dependencies file for $*.o
	@$(SHELL) -ec '$(CC) -MM $(CFLAGS) $(INCLUDE) $< | sed "s^$*.o^$(UTILS)/../$(TARGETDIR)$*.o^" > $@'

$(APP)/../.dep/%.d: %.cpp
	@mkdir -p $(dir $@)
	@echo "============="
	@echo Building dependencies file for $*.o
	@$(SHELL) -ec '$(CC) -MM $(CFLAGS) $(INCLUDE) $< | sed "s^$*.o^$(APP)/../$(TARGETDIR)$*.o^" > $@'

$(SERIAL)/../.dep/%.d: %.cpp
	@mkdir -p $(dir $@)
	@echo "============="
	@echo Building dependencies file for $*.o
	@$(SHELL) -ec '$(CC) -MM $(CFLAGS) $(INCLUDE) $< | sed "s^$*.o^$(SERIAL)/../$(TARGETDIR)$*.o^" > $@'


## Include the dependency files
include $(DEPENDS)
