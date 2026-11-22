BUILD_DIR ?= build
PYTHON    ?= python3

.PHONY: all build test sweep plot regress ref clean

all: build test

build:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR) -j

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

ref: build
	$(PYTHON) scripts/ref_tlb.py --binary $(BUILD_DIR)/translation_sim --verify-suite

regress: build
	$(PYTHON) scripts/regress.py --binary $(BUILD_DIR)/translation_sim --configs-dir configs --golden-dir results/golden

sweep: build
	$(PYTHON) scripts/run_sweep.py --binary $(BUILD_DIR)/translation_sim --grid configs/sweep_grid.json --out-dir results/runs

plot: sweep
	$(PYTHON) scripts/plot.py --input results/runs/sweep_results.csv --out-dir docs/figures

clean:
	rm -rf $(BUILD_DIR) results/runs
