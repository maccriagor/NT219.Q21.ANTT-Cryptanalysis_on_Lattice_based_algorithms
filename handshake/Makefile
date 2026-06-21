# Define phony targets to prevent conflicts with files of the same name
.PHONY: all clean  run_scripts config_openssl 

# Path to the openssl config file
OPENSSL_CNF = openssl-build/ssl/openssl.cnf

# The default target that runs the entire pipeline in the correct logical sequence
all: run_scripts config_openssl 




# 2. Run the two setup scripts in explicit order
run_scripts:
	@echo "Running setup scripts"
	chmod +x gen_certs.sh setup_oqs_openssl.sh
	./gen_certs.sh
	./setup_oqs_openssl.sh

# 3. Modify the openssl.cnf file safely
config_openssl:
	@echo "Checking and modifying $(OPENSSL_CNF)..."
	@if [ -f $(OPENSSL_CNF) ]; then \
		if grep -q "oqs = oqs_sect" $(OPENSSL_CNF); then \
			echo "openssl.cnf is already configured. Skipping."; \
		else \
			echo "Updating provider block at the bottom of $(OPENSSL_CNF)..."; \
			/* Delete from [provider_sect] to the end of the file to clear any stale block */ \
			sed -i '/\[provider_sect\]/,$$d' $(OPENSSL_CNF); \
			/* Append the pristine configuration blocks */ \
			echo "[provider_sect]" >> $(OPENSSL_CNF); \
			echo "default = default_sect" >> $(OPENSSL_CNF); \
			echo "oqs = oqs_sect" >> $(OPENSSL_CNF); \
			echo "" >> $(OPENSSL_CNF); \
			echo "[default_sect]" >> $(OPENSSL_CNF); \
			echo "activate = 1" >> $(OPENSSL_CNF); \
			echo "" >> $(OPENSSL_CNF); \
			echo "[oqs_sect]" >> $(OPENSSL_CNF); \
			echo "module = oqsprovider.so" >> $(OPENSSL_CNF); \
			echo "activate = 1" >> $(OPENSSL_CNF); \
		fi \
	else \
		echo "Error: $(OPENSSL_CNF) not found! Did setup_oqs_openssl.sh fail?"; \
		exit 1; \
	fi


