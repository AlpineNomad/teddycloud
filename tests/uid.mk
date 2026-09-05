.PHONY: test-uid
test-uid: bin/test-uid
	./bin/test-uid

bin/test-uid: $(filter-out obj/src/main.o,$(OBJECTS)) obj/tests/test_uid.o
	$(CC) $(LFLAGS) $^ -o $@
