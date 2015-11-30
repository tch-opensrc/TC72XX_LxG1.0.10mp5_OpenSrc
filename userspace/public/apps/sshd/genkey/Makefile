ifeq ($(CC),gcc)
	GENKEY = $(HOST_GENKEY)
else
	GENKEY = $(MIPS_GENKEY)			
endif

CRYPT_SRC=$(SSHD_DIR)/libtomcrypt/src
CRYPT_HEADERS=$(CRYPT_SRC)/headers
MATH_DIR=$(SSHD_DIR)/libtommath
	
CFLAGS=-Wall -W -O2 -DSSHD_GENKEY -DDROPBEAR_SERVER -I$(CRYPT_HEADERS) -I$(MATH_DIR) -I.. -I$(BUILD_DIR)/userspace/public/include -I$(BUILD_DIR)/userspace/public/include/linux
LIBS= -lcrypt -lutil

HEADERS=options.h dbutil.h session.h packet.h algo.h ssh.h buffer.h kex.h \
		dss.h bignum.h signkey.h rsa.h random.h service.h auth.h \
		debug.h channel.h chansession.h config.h queue.h sshpty.h \
		termcodes.h gendss.h genrsa.h runopts.h includes.h \
		loginrec.h atomicio.h x11fwd.h agentfwd.h tcpfwd.h compat.h \
		listener.h fake-rfc2553.h

COMMONOBJS=dbutil.o buffer.o \
		dss.o bignum.o \
		signkey.o rsa.o random.o \
		queue.o \
		atomicio.o compat.o  fake-rfc2553.o

MATH_OBJS=bncore.o bn_mp_init.o bn_mp_clear.o bn_mp_exch.o bn_mp_grow.o bn_mp_shrink.o \
bn_mp_clamp.o bn_mp_zero.o  bn_mp_set.o bn_mp_set_int.o bn_mp_init_size.o bn_mp_copy.o \
bn_mp_init_copy.o bn_mp_abs.o bn_mp_neg.o bn_mp_cmp_mag.o bn_mp_cmp.o bn_mp_cmp_d.o \
bn_mp_rshd.o bn_mp_lshd.o bn_mp_mod_2d.o bn_mp_div_2d.o bn_mp_mul_2d.o bn_mp_div_2.o \
bn_mp_mul_2.o bn_s_mp_add.o bn_s_mp_sub.o bn_fast_s_mp_mul_digs.o bn_s_mp_mul_digs.o \
bn_fast_s_mp_mul_high_digs.o bn_s_mp_mul_high_digs.o bn_fast_s_mp_sqr.o bn_s_mp_sqr.o \
bn_mp_add.o bn_mp_sub.o bn_mp_karatsuba_mul.o bn_mp_mul.o bn_mp_karatsuba_sqr.o \
bn_mp_sqr.o bn_mp_div.o bn_mp_mod.o bn_mp_add_d.o bn_mp_sub_d.o bn_mp_mul_d.o \
bn_mp_div_d.o bn_mp_mod_d.o bn_mp_expt_d.o bn_mp_addmod.o bn_mp_submod.o \
bn_mp_mulmod.o bn_mp_sqrmod.o bn_mp_gcd.o bn_mp_lcm.o bn_fast_mp_invmod.o bn_mp_invmod.o \
bn_mp_reduce.o bn_mp_montgomery_setup.o bn_fast_mp_montgomery_reduce.o bn_mp_montgomery_reduce.o \
bn_mp_exptmod_fast.o bn_mp_exptmod.o bn_mp_2expt.o bn_mp_n_root.o bn_mp_jacobi.o bn_reverse.o \
bn_mp_count_bits.o bn_mp_read_unsigned_bin.o bn_mp_read_signed_bin.o bn_mp_to_unsigned_bin.o \
bn_mp_to_signed_bin.o bn_mp_unsigned_bin_size.o bn_mp_signed_bin_size.o  \
bn_mp_xor.o bn_mp_and.o bn_mp_or.o bn_mp_rand.o bn_mp_montgomery_calc_normalization.o \
bn_mp_prime_is_divisible.o bn_prime_tab.o bn_mp_prime_fermat.o bn_mp_prime_miller_rabin.o \
bn_mp_prime_is_prime.o bn_mp_prime_next_prime.o bn_mp_dr_reduce.o \
bn_mp_dr_is_modulus.o bn_mp_dr_setup.o bn_mp_reduce_setup.o \
bn_mp_toom_mul.o bn_mp_toom_sqr.o bn_mp_div_3.o bn_s_mp_exptmod.o \
bn_mp_reduce_2k.o bn_mp_reduce_is_2k.o bn_mp_reduce_2k_setup.o \
bn_mp_reduce_2k_l.o bn_mp_reduce_is_2k_l.o bn_mp_reduce_2k_setup_l.o \
bn_mp_radix_smap.o bn_mp_read_radix.o bn_mp_toradix.o bn_mp_radix_size.o \
bn_mp_fread.o bn_mp_fwrite.o bn_mp_cnt_lsb.o bn_error.o \
bn_mp_init_multi.o bn_mp_clear_multi.o bn_mp_exteuclid.o bn_mp_toradix_n.o \
bn_mp_prime_random_ex.o bn_mp_get_int.o bn_mp_sqrt.o bn_mp_is_square.o bn_mp_init_set.o \
bn_mp_init_set_int.o bn_mp_invmod_slow.o bn_mp_prime_rabin_miller_trials.o \
bn_mp_to_signed_bin_n.o bn_mp_to_unsigned_bin_n.o

HELPER=$(CRYPT_SRC)/hashes/helper
HASHES=$(CRYPT_SRC)/hashes
BASE64=$(CRYPT_SRC)/misc/base64
MISC  =$(CRYPT_SRC)/misc
CRYPT =$(CRYPT_SRC)/misc/crypt

CRYPTO_OBJS = hash_file.o hash_filehandle.o \
hash_memory.o hash_memory_multi.o \
md5.o sha1.o \
burn_stack.o crypt.o \
crypt_argchk.o crypt_cipher_descriptor.o \
crypt_cipher_is_valid.o crypt_find_cipher.o \
crypt_find_cipher_any.o crypt_find_cipher_id.o \
crypt_find_hash.o crypt_find_hash_any.o \
crypt_find_hash_id.o crypt_find_prng.o \
crypt_hash_descriptor.o crypt_hash_is_valid.o \
crypt_prng_descriptor.o crypt_prng_is_valid.o \
crypt_register_cipher.o crypt_register_hash.o \
crypt_register_prng.o crypt_unregister_cipher.o \
crypt_unregister_hash.o crypt_unregister_prng.o \
error_to_string.o base64_decode.o base64_encode.o zeromem.o\

KEYOBJS=dropbearkey.o gendss.o genrsa.o

vpath %.h $(SSHD_DIR):$(CRYPT_HEADERS):$(MATH_DIR)
vpath %.c $(SSHD_DIR):$(CRYPT_HEADERS):$(MATH_DIR):$(BASE64):$(HELPER):$(HASHES):$(MISC):$(CRYPT)

$(GENKEY): $(COMMONOBJS) $(MATH_OBJS) $(CRYPTO_OBJS) $(KEYOBJS) $(HEADERS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(GENKEY) $(COMMONOBJS) $(MATH_OBJS) $(CRYPTO_OBJS) $(KEYOBJS) $(LIBS)	
	$(STRIP) $(GENKEY)
	  
clean: 
	-rm -f $(GENKEY) *.o
