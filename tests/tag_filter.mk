.PHONY: test-tag-filter
test-tag-filter: bin/test-tag-filter
	./bin/test-tag-filter

bin/test-tag-filter: $(filter-out obj/src/main.o,$(OBJECTS)) obj/tests/test_tag_filter.o
	$(CC) $(LFLAGS) $^ -o $@
