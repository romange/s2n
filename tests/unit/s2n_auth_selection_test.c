/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <s2n.h>

#include "s2n_test.h"
#include "testlib/s2n_testlib.h"

#include "crypto/s2n_fips.h"

#include "tls/s2n_auth_selection.h"
#include "tls/s2n_cipher_preferences.h"
#include "tls/s2n_cipher_suites.h"
#include "tls/s2n_config.h"
#include "tls/s2n_connection.h"
#include "tls/s2n_signature_scheme.h"

#define RSA_AUTH_CIPHER_SUITE &s2n_dhe_rsa_with_3des_ede_cbc_sha
#define ECDSA_AUTH_CIPHER_SUITE &s2n_ecdhe_ecdsa_with_aes_128_cbc_sha
#define NO_AUTH_CIPHER_SUITE &s2n_tls13_aes_128_gcm_sha256

#if RSA_PSS_SUPPORTED
#define EXPECT_SUCCESS_IF_RSA_PSS_SUPPORTED(x) EXPECT_SUCCESS(x)
#else
#define EXPECT_SUCCESS_IF_RSA_PSS_SUPPORTED(x) EXPECT_FAILURE(x)
#endif

static int s2n_test_auth_combo(struct s2n_connection *conn,
        struct s2n_cipher_suite *cipher_suite, s2n_signature_algorithm sig_alg,
        struct s2n_cert_chain_and_key *expected_cert_chain)
{
    struct s2n_cert_chain_and_key *actual_cert_chain;

    GUARD(s2n_is_cipher_suite_valid_for_auth(conn, cipher_suite));
    conn->secure.cipher_suite = cipher_suite;

    GUARD(s2n_is_sig_alg_valid_for_auth(conn, sig_alg));
    conn->secure.conn_sig_scheme.sig_alg = sig_alg;

    GUARD(s2n_select_certs_for_server_auth(conn, &actual_cert_chain));
    eq_check(actual_cert_chain, expected_cert_chain);
    return S2N_SUCCESS;
}

int main(int argc, char **argv)
{
    BEGIN_TEST();

    struct s2n_cert_chain_and_key *rsa_cert_chain;
    EXPECT_SUCCESS(s2n_test_cert_chain_and_key_new(&rsa_cert_chain,
            S2N_RSA_2048_PKCS1_CERT_CHAIN, S2N_RSA_2048_PKCS1_KEY));

    struct s2n_cert_chain_and_key *ecdsa_cert_chain;
    EXPECT_SUCCESS(s2n_test_cert_chain_and_key_new(&ecdsa_cert_chain,
            S2N_ECDSA_P384_PKCS1_CERT_CHAIN, S2N_ECDSA_P384_PKCS1_KEY));

    struct s2n_config *no_certs_config = s2n_config_new();

    struct s2n_config *rsa_cert_config = s2n_config_new();
    EXPECT_SUCCESS(s2n_config_add_cert_chain_and_key_to_store(rsa_cert_config, rsa_cert_chain));

    struct s2n_config *ecdsa_cert_config = s2n_config_new();
    EXPECT_SUCCESS(s2n_config_add_cert_chain_and_key_to_store(ecdsa_cert_config, ecdsa_cert_chain));

    struct s2n_config *all_certs_config = s2n_config_new();
    EXPECT_SUCCESS(s2n_config_add_cert_chain_and_key_to_store(all_certs_config, rsa_cert_chain));
    EXPECT_SUCCESS(s2n_config_add_cert_chain_and_key_to_store(all_certs_config, ecdsa_cert_chain));

    struct s2n_cert_chain_and_key *rsa_pss_cert_chain = NULL;
    struct s2n_config *rsa_pss_cert_config = s2n_config_new();
#if RSA_PSS_SUPPORTED
    EXPECT_SUCCESS(s2n_test_cert_chain_and_key_new(&rsa_pss_cert_chain,
            S2N_RSA_PSS_2048_SHA256_LEAF_CERT, S2N_RSA_PSS_2048_SHA256_LEAF_KEY));
    EXPECT_SUCCESS(s2n_config_add_cert_chain_and_key_to_store(rsa_pss_cert_config, rsa_pss_cert_chain));
    EXPECT_SUCCESS(s2n_config_add_cert_chain_and_key_to_store(all_certs_config, rsa_pss_cert_chain));
#endif

    /* s2n_is_cipher_suite_valid_for_auth */
    {
        struct s2n_connection *conn = s2n_connection_new(S2N_SERVER);

        /* Test: not valid if certs not available */
        {
            /* No certs exist */
            s2n_connection_set_config(conn, no_certs_config);
            EXPECT_FAILURE(s2n_is_cipher_suite_valid_for_auth(conn, RSA_AUTH_CIPHER_SUITE));
            EXPECT_FAILURE(s2n_is_cipher_suite_valid_for_auth(conn, ECDSA_AUTH_CIPHER_SUITE));
            EXPECT_FAILURE(s2n_is_cipher_suite_valid_for_auth(conn, NO_AUTH_CIPHER_SUITE));

            /* RSA certs exist */
            s2n_connection_set_config(conn, rsa_cert_config);
            EXPECT_SUCCESS(s2n_is_cipher_suite_valid_for_auth(conn, RSA_AUTH_CIPHER_SUITE));
            EXPECT_FAILURE(s2n_is_cipher_suite_valid_for_auth(conn, ECDSA_AUTH_CIPHER_SUITE));
            EXPECT_SUCCESS(s2n_is_cipher_suite_valid_for_auth(conn, NO_AUTH_CIPHER_SUITE));

            /* RSA-PSS certs exist */
            s2n_connection_set_config(conn, rsa_pss_cert_config);
            EXPECT_SUCCESS_IF_RSA_PSS_SUPPORTED(s2n_is_cipher_suite_valid_for_auth(conn, RSA_AUTH_CIPHER_SUITE));
            EXPECT_FAILURE(s2n_is_cipher_suite_valid_for_auth(conn, ECDSA_AUTH_CIPHER_SUITE));
            EXPECT_SUCCESS_IF_RSA_PSS_SUPPORTED(s2n_is_cipher_suite_valid_for_auth(conn, NO_AUTH_CIPHER_SUITE));

            /* ECDSA certs exist */
            s2n_connection_set_config(conn, ecdsa_cert_config);
            EXPECT_FAILURE(s2n_is_cipher_suite_valid_for_auth(conn, RSA_AUTH_CIPHER_SUITE));
            EXPECT_SUCCESS(s2n_is_cipher_suite_valid_for_auth(conn, ECDSA_AUTH_CIPHER_SUITE));
            EXPECT_SUCCESS(s2n_is_cipher_suite_valid_for_auth(conn, NO_AUTH_CIPHER_SUITE));

            /* All certs exist */
            s2n_connection_set_config(conn, all_certs_config);
            EXPECT_SUCCESS(s2n_is_cipher_suite_valid_for_auth(conn, RSA_AUTH_CIPHER_SUITE));
            EXPECT_SUCCESS(s2n_is_cipher_suite_valid_for_auth(conn, ECDSA_AUTH_CIPHER_SUITE));
            EXPECT_SUCCESS(s2n_is_cipher_suite_valid_for_auth(conn, NO_AUTH_CIPHER_SUITE));
        }

        EXPECT_SUCCESS(s2n_connection_free(conn));
    }

    /* s2n_is_sig_alg_valid_for_auth */
    {
        struct s2n_connection *conn = s2n_connection_new(S2N_SERVER);

        /* Test: not valid if certs not available */
        {
            conn->secure.cipher_suite = NO_AUTH_CIPHER_SUITE;

            /* No certs exist */
            s2n_connection_set_config(conn, no_certs_config);
            EXPECT_FAILURE(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA));
            EXPECT_FAILURE(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA_PSS_PSS));
            EXPECT_FAILURE(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA_PSS_RSAE));
            EXPECT_FAILURE(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_ECDSA));

            /* RSA certs exist */
            s2n_connection_set_config(conn, rsa_cert_config);
            EXPECT_SUCCESS(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA));
            EXPECT_FAILURE(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA_PSS_PSS));
            EXPECT_SUCCESS(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA_PSS_RSAE));
            EXPECT_FAILURE(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_ECDSA));

            /* RSA-PSS certs exist */
            s2n_connection_set_config(conn, rsa_pss_cert_config);
            EXPECT_FAILURE(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA));
            EXPECT_SUCCESS_IF_RSA_PSS_SUPPORTED(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA_PSS_PSS));
            EXPECT_FAILURE(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA_PSS_RSAE));
            EXPECT_FAILURE(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_ECDSA));

            /* ECDSA certs exist */
            s2n_connection_set_config(conn, ecdsa_cert_config);
            EXPECT_FAILURE(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA));
            EXPECT_FAILURE(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA_PSS_PSS));
            EXPECT_FAILURE(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA_PSS_RSAE));
            EXPECT_SUCCESS(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_ECDSA));

            /* All certs exist */
            s2n_connection_set_config(conn, all_certs_config);
            EXPECT_SUCCESS(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA));
            EXPECT_SUCCESS_IF_RSA_PSS_SUPPORTED(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA_PSS_PSS));
            EXPECT_SUCCESS(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA_PSS_RSAE));
            EXPECT_SUCCESS(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_ECDSA));
        }

        /* Test: If cipher suite specifies auth type, auth type must be valid for sig alg */
        {
            s2n_connection_set_config(conn, all_certs_config);

            /* RSA auth type */
            conn->secure.cipher_suite = RSA_AUTH_CIPHER_SUITE;
            EXPECT_SUCCESS(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA));
            EXPECT_SUCCESS_IF_RSA_PSS_SUPPORTED(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA_PSS_PSS));
            EXPECT_SUCCESS(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA_PSS_RSAE));
            EXPECT_FAILURE(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_ECDSA));

            /* ECDSA auth type */
            conn->secure.cipher_suite = ECDSA_AUTH_CIPHER_SUITE;
            EXPECT_FAILURE(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA));
            EXPECT_FAILURE(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA_PSS_PSS));
            EXPECT_FAILURE(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA_PSS_RSAE));
            EXPECT_SUCCESS(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_ECDSA));
        }

        /* Test: RSA-PSS requires a non-ephemeral kex */
        {
            s2n_connection_set_config(conn, all_certs_config);

            /* ephemeral key */
            conn->secure.cipher_suite = &s2n_dhe_rsa_with_3des_ede_cbc_sha; /* kex = (dhe) */
            EXPECT_SUCCESS(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA));
            EXPECT_SUCCESS_IF_RSA_PSS_SUPPORTED(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA_PSS_PSS));
            EXPECT_SUCCESS(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA_PSS_RSAE));

            /* non-ephemeral key */
            conn->secure.cipher_suite = &s2n_rsa_with_rc4_128_md5; /* kex = (rsa) */
            EXPECT_SUCCESS(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA));
            EXPECT_FAILURE(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA_PSS_PSS));
            EXPECT_SUCCESS(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA_PSS_RSAE));

            /* no kex at all */
            conn->secure.cipher_suite = NO_AUTH_CIPHER_SUITE; /* kex = NULL */
            EXPECT_SUCCESS(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA));
            EXPECT_SUCCESS_IF_RSA_PSS_SUPPORTED(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA_PSS_PSS));
            EXPECT_SUCCESS(s2n_is_sig_alg_valid_for_auth(conn, S2N_SIGNATURE_RSA_PSS_RSAE));
        }

        EXPECT_SUCCESS(s2n_connection_free(conn));
    }

    /* s2n_is_cert_type_valid_for_auth */
    {
        struct s2n_connection *conn = s2n_connection_new(S2N_CLIENT);

        /* RSA auth type */
        conn->secure.cipher_suite = RSA_AUTH_CIPHER_SUITE;
        EXPECT_SUCCESS(s2n_is_cert_type_valid_for_auth(conn, S2N_PKEY_TYPE_RSA));
        EXPECT_SUCCESS(s2n_is_cert_type_valid_for_auth(conn, S2N_PKEY_TYPE_RSA_PSS));
        EXPECT_FAILURE(s2n_is_cert_type_valid_for_auth(conn, S2N_PKEY_TYPE_ECDSA));
        EXPECT_FAILURE(s2n_is_cert_type_valid_for_auth(conn, S2N_PKEY_TYPE_UNKNOWN));

        /* ECDSA auth type */
        conn->secure.cipher_suite = ECDSA_AUTH_CIPHER_SUITE;
        EXPECT_FAILURE(s2n_is_cert_type_valid_for_auth(conn, S2N_PKEY_TYPE_RSA));
        EXPECT_FAILURE(s2n_is_cert_type_valid_for_auth(conn, S2N_PKEY_TYPE_RSA_PSS));
        EXPECT_SUCCESS(s2n_is_cert_type_valid_for_auth(conn, S2N_PKEY_TYPE_ECDSA));
        EXPECT_FAILURE(s2n_is_cert_type_valid_for_auth(conn, S2N_PKEY_TYPE_UNKNOWN));

        /* No auth type */
        conn->secure.cipher_suite = NO_AUTH_CIPHER_SUITE;
        EXPECT_SUCCESS(s2n_is_cert_type_valid_for_auth(conn, S2N_PKEY_TYPE_RSA));
        EXPECT_SUCCESS(s2n_is_cert_type_valid_for_auth(conn, S2N_PKEY_TYPE_RSA_PSS));
        EXPECT_SUCCESS(s2n_is_cert_type_valid_for_auth(conn, S2N_PKEY_TYPE_ECDSA));
        EXPECT_FAILURE(s2n_is_cert_type_valid_for_auth(conn, S2N_PKEY_TYPE_UNKNOWN));

        EXPECT_SUCCESS(s2n_connection_free(conn));
    }

    /* s2n_select_certs_for_server_auth */
    {
        struct s2n_connection *conn = s2n_connection_new(S2N_SERVER);
        struct s2n_cert_chain_and_key *chosen_certs;

        /* Requested cert chain exists */
        s2n_connection_set_config(conn, all_certs_config);

        conn->secure.conn_sig_scheme.sig_alg = S2N_SIGNATURE_RSA;
        EXPECT_SUCCESS(s2n_select_certs_for_server_auth(conn, &chosen_certs));
        EXPECT_EQUAL(chosen_certs, rsa_cert_chain);

        conn->secure.conn_sig_scheme.sig_alg = S2N_SIGNATURE_RSA_PSS_PSS;
        EXPECT_SUCCESS_IF_RSA_PSS_SUPPORTED(s2n_select_certs_for_server_auth(conn, &chosen_certs));
        EXPECT_EQUAL(chosen_certs, rsa_pss_cert_chain);

        conn->secure.conn_sig_scheme.sig_alg = S2N_SIGNATURE_RSA_PSS_RSAE;
        EXPECT_SUCCESS(s2n_select_certs_for_server_auth(conn, &chosen_certs));
        EXPECT_EQUAL(chosen_certs, rsa_cert_chain);

        conn->secure.conn_sig_scheme.sig_alg = S2N_SIGNATURE_ECDSA;
        EXPECT_SUCCESS(s2n_select_certs_for_server_auth(conn, &chosen_certs));
        EXPECT_EQUAL(chosen_certs, ecdsa_cert_chain);

        /* Requested cert chain does NOT exist */
        s2n_connection_set_config(conn, no_certs_config);

        conn->secure.conn_sig_scheme.sig_alg = S2N_SIGNATURE_RSA;
        EXPECT_FAILURE(s2n_select_certs_for_server_auth(conn, &chosen_certs));
        EXPECT_NULL(chosen_certs);

        conn->secure.conn_sig_scheme.sig_alg = S2N_SIGNATURE_RSA_PSS_PSS;
        EXPECT_FAILURE(s2n_select_certs_for_server_auth(conn, &chosen_certs));
        EXPECT_NULL(chosen_certs);

        conn->secure.conn_sig_scheme.sig_alg = S2N_SIGNATURE_RSA_PSS_RSAE;
        EXPECT_FAILURE(s2n_select_certs_for_server_auth(conn, &chosen_certs));
        EXPECT_NULL(chosen_certs);

        conn->secure.conn_sig_scheme.sig_alg = S2N_SIGNATURE_ECDSA;
        EXPECT_FAILURE(s2n_select_certs_for_server_auth(conn, &chosen_certs));
        EXPECT_NULL(chosen_certs);

        EXPECT_SUCCESS(s2n_connection_free(conn));
    }

    /* Test all possible combos */
    {
        struct s2n_connection *conn = s2n_connection_new(S2N_SERVER);
        s2n_connection_set_config(conn, all_certs_config);

        /* No certs exist */
        s2n_connection_set_config(conn, no_certs_config);

        EXPECT_FAILURE(s2n_test_auth_combo(conn, RSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA, NULL));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, RSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_PSS, NULL));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, RSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_RSAE, NULL));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, RSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_ECDSA, NULL));

        EXPECT_FAILURE(s2n_test_auth_combo(conn, ECDSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA, NULL));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, ECDSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_PSS, NULL));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, ECDSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_RSAE, NULL));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, ECDSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_ECDSA, NULL));

        EXPECT_FAILURE(s2n_test_auth_combo(conn, NO_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA, NULL));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, NO_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_PSS, NULL));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, NO_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_RSAE, NULL));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, NO_AUTH_CIPHER_SUITE, S2N_SIGNATURE_ECDSA, NULL));

        /* RSA certs exist */
        s2n_connection_set_config(conn, rsa_cert_config);

        EXPECT_SUCCESS(s2n_test_auth_combo(conn, RSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA, rsa_cert_chain));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, RSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_PSS, NULL));
        EXPECT_SUCCESS(s2n_test_auth_combo(conn, RSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_RSAE, rsa_cert_chain));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, RSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_ECDSA, NULL));

        EXPECT_FAILURE(s2n_test_auth_combo(conn, ECDSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA, NULL));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, ECDSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_PSS, NULL));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, ECDSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_RSAE, NULL));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, ECDSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_ECDSA, NULL));

        EXPECT_SUCCESS(s2n_test_auth_combo(conn, NO_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA, rsa_cert_chain));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, NO_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_PSS, NULL));
        EXPECT_SUCCESS(s2n_test_auth_combo(conn, NO_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_RSAE, rsa_cert_chain));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, NO_AUTH_CIPHER_SUITE, S2N_SIGNATURE_ECDSA, NULL));

        /* RSA_PSS certs exist */
        s2n_connection_set_config(conn, rsa_pss_cert_config);

        EXPECT_FAILURE(s2n_test_auth_combo(conn, RSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA, NULL));
        EXPECT_SUCCESS_IF_RSA_PSS_SUPPORTED(s2n_test_auth_combo(conn, RSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_PSS, rsa_pss_cert_chain));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, RSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_RSAE, NULL));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, RSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_ECDSA, NULL));

        EXPECT_FAILURE(s2n_test_auth_combo(conn, ECDSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA, NULL));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, ECDSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_PSS, NULL));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, ECDSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_RSAE, NULL));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, ECDSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_ECDSA, NULL));

        EXPECT_FAILURE(s2n_test_auth_combo(conn, NO_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA, NULL));
        EXPECT_SUCCESS_IF_RSA_PSS_SUPPORTED(s2n_test_auth_combo(conn, NO_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_PSS, rsa_pss_cert_chain));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, NO_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_RSAE, NULL));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, NO_AUTH_CIPHER_SUITE, S2N_SIGNATURE_ECDSA, NULL));

        /* ECDSA certs exist */
        s2n_connection_set_config(conn, ecdsa_cert_config);

        EXPECT_FAILURE(s2n_test_auth_combo(conn, RSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA, NULL));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, RSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_PSS, NULL));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, RSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_RSAE, NULL));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, RSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_ECDSA, NULL));

        EXPECT_FAILURE(s2n_test_auth_combo(conn, ECDSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA, NULL));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, ECDSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_PSS, NULL));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, ECDSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_RSAE, NULL));
        EXPECT_SUCCESS(s2n_test_auth_combo(conn, ECDSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_ECDSA, ecdsa_cert_chain));

        EXPECT_FAILURE(s2n_test_auth_combo(conn, NO_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA, NULL));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, NO_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_PSS, NULL));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, NO_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_RSAE, NULL));
        EXPECT_SUCCESS(s2n_test_auth_combo(conn, NO_AUTH_CIPHER_SUITE, S2N_SIGNATURE_ECDSA, ecdsa_cert_chain));

        /* All certs exist */
        s2n_connection_set_config(conn, all_certs_config);

        EXPECT_SUCCESS(s2n_test_auth_combo(conn, RSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA, rsa_cert_chain));
        EXPECT_SUCCESS_IF_RSA_PSS_SUPPORTED(s2n_test_auth_combo(conn, RSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_PSS, rsa_pss_cert_chain));
        EXPECT_SUCCESS(s2n_test_auth_combo(conn, RSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_RSAE, rsa_cert_chain));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, RSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_ECDSA, ecdsa_cert_chain));

        EXPECT_FAILURE(s2n_test_auth_combo(conn, ECDSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA, rsa_cert_chain));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, ECDSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_PSS, rsa_pss_cert_chain));
        EXPECT_FAILURE(s2n_test_auth_combo(conn, ECDSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_RSAE, rsa_cert_chain));
        EXPECT_SUCCESS(s2n_test_auth_combo(conn, ECDSA_AUTH_CIPHER_SUITE, S2N_SIGNATURE_ECDSA, ecdsa_cert_chain));

        EXPECT_SUCCESS(s2n_test_auth_combo(conn, NO_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA, rsa_cert_chain));
        EXPECT_SUCCESS_IF_RSA_PSS_SUPPORTED(s2n_test_auth_combo(conn, NO_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_PSS, rsa_pss_cert_chain));
        EXPECT_SUCCESS(s2n_test_auth_combo(conn, NO_AUTH_CIPHER_SUITE, S2N_SIGNATURE_RSA_PSS_RSAE, rsa_cert_chain));
        EXPECT_SUCCESS(s2n_test_auth_combo(conn, NO_AUTH_CIPHER_SUITE, S2N_SIGNATURE_ECDSA, ecdsa_cert_chain));

        EXPECT_SUCCESS(s2n_connection_free(conn));
    }

    EXPECT_SUCCESS(s2n_config_free(rsa_cert_config));
    EXPECT_SUCCESS(s2n_config_free(rsa_pss_cert_config));
    EXPECT_SUCCESS(s2n_config_free(ecdsa_cert_config));
    EXPECT_SUCCESS(s2n_config_free(all_certs_config));
    EXPECT_SUCCESS(s2n_config_free(no_certs_config));
    EXPECT_SUCCESS(s2n_cert_chain_and_key_free(rsa_cert_chain));
    EXPECT_SUCCESS(s2n_cert_chain_and_key_free(rsa_pss_cert_chain));
    EXPECT_SUCCESS(s2n_cert_chain_and_key_free(ecdsa_cert_chain));

    END_TEST();

    return 0;
}