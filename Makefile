BUILD_DIR ?= build
SRC_DIR   ?= src
T         ?= /tmp

all: unwrap

unwrap: $(BUILD_DIR)/Makefile
	$(MAKE) -C $(BUILD_DIR)
	ln -sf unwrap wrap

$(BUILD_DIR)/Makefile: CMakeLists.txt $(SRC_DIR)/*.cpp $(SRC_DIR)/*.h
	mkdir -p $(BUILD_DIR)
	cmake -S . -B $(BUILD_DIR) -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=$(CURDIR)

.PHONY: test
test: unwrap
	@# Roundtrip: wrap a short function, unwrap it, compare (comments stripped, uppercased)
	@printf 'CREATE OR REPLACE FUNCTION f RETURN NUMBER IS\n  x NUMBER := 42; /* magic */\nBEGIN\n  RETURN x;\nEND;\n' >$(T)/_ut_src.sql
	./unwrap --wrap -i $(T)/_ut_src.sql -o $(T)/_ut_wrapped.sql 2>&1
	@./unwrap -i $(T)/_ut_wrapped.sql >$(T)/_ut_roundtrip.sql 2>&1
	@diff -q $(T)/_ut_src.sql $(T)/_ut_roundtrip.sql >/dev/null 2>&1 && echo "FAIL: roundtrip should differ (comments stripped)" && exit 1 || true
	@grep -q 'wrapped' $(T)/_ut_wrapped.sql || { echo "FAIL: wrapped output missing marker"; exit 1; }
	@grep -q 'CREATE OR REPLACE FUNCTION f' $(T)/_ut_roundtrip.sql || { echo "FAIL: roundtrip output broken"; exit 1; }
	@# keep_comments: comments should survive roundtrip
	./unwrap --wrap --keep-comments -i $(T)/_ut_src.sql -o $(T)/_ut_wrapped2.sql 2>&1
	@./unwrap -i $(T)/_ut_wrapped2.sql >$(T)/_ut_roundtrip2.sql 2>&1
	@grep -q 'MAGIC' $(T)/_ut_roundtrip2.sql || { echo "FAIL: keep_comments lost the comment"; exit 1; }
	@rm -f $(T)/_ut_*.sql
	@# Obfuscation roundtrip
	@printf 'CREATE OR REPLACE PROCEDURE test_obf IS\n  v_counter NUMBER := 0;\nBEGIN\n  v_counter := v_counter + 1;\nEND;\n' >$(T)/_ut_obf_src.sql
	./unwrap --obf -p test123 -i $(T)/_ut_obf_src.sql -o $(T)/_ut_obf_out.sql 2>&1
	@grep -q '\-\-ENC' $(T)/_ut_obf_out.sql || { echo "FAIL: obfuscation missing ENC header"; exit 1; }
	@grep -q 'v_counter' $(T)/_ut_obf_out.sql && { echo "FAIL: obfuscation left original name"; exit 1; } || true
	./unwrap --deobf -p test123 -i $(T)/_ut_obf_out.sql -o $(T)/_ut_obf_restored.sql 2>&1
	@grep -q 'v_counter' $(T)/_ut_obf_restored.sql || { echo "FAIL: deobfuscation did not restore names"; exit 1; }
	@# Wrong passphrase should fail
	./unwrap --deobf -p wrong -i $(T)/_ut_obf_out.sql >/dev/null 2>&1; if [ $$? = 0 ]; then echo "FAIL: wrong passphrase should fail"; exit 1; fi
	@rm -f $(T)/_ut_*.sql $(T)/_ut_obf_*.sql
	@echo "All tests passed."

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	rm -f unwrap wrap
	rm -f $(SRC_DIR)/*~ $(SRC_DIR)/*.orig
