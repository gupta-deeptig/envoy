#include <string>
#include <vector>

#include "source/extensions/transport_sockets/tls/cert_validator/default_validator.h"
#include "source/extensions/transport_sockets/tls/cert_validator/san_matcher.h"

#include "test/extensions/transport_sockets/tls/cert_validator/test_common.h"
#include "test/extensions/transport_sockets/tls/ssl_test_utility.h"
#include "test/test_common/environment.h"
#include "test/test_common/test_runtime.h"
#include "test/test_common/utility.h"

#include "gtest/gtest.h"
#include "openssl/ssl.h"
#include "openssl/x509v3.h"

namespace Envoy {
namespace Extensions {
namespace TransportSockets {
namespace Tls {

using TestCertificateValidationContextConfigPtr =
    std::unique_ptr<TestCertificateValidationContextConfig>;
using X509StoreContextPtr = CSmartPtr<X509_STORE_CTX, X509_STORE_CTX_free>;
using X509StorePtr = CSmartPtr<X509_STORE, X509_STORE_free>;
using SSLContextPtr = CSmartPtr<SSL_CTX, SSL_CTX_free>;

TEST(DefaultCertValidatorTest, TestVerifySubjectAltNameDNSMatched) {
  bssl::UniquePtr<X509> cert = readCertFromFile(TestEnvironment::substitute(
      "{{ test_rundir }}/test/extensions/transport_sockets/tls/test_data/san_dns_cert.pem"));
  std::vector<std::string> verify_subject_alt_name_list = {"server1.example.com",
                                                           "server2.example.com"};
  EXPECT_TRUE(DefaultCertValidator::verifySubjectAltName(cert.get(), verify_subject_alt_name_list));
}

TEST(DefaultCertValidatorTest, TestMatchSubjectAltNameDNSMatched) {
  bssl::UniquePtr<X509> cert = readCertFromFile(TestEnvironment::substitute(
      "{{ test_rundir }}/test/extensions/transport_sockets/tls/test_data/san_dns_cert.pem"));
  envoy::type::matcher::v3::StringMatcher matcher;
  matcher.MergeFrom(TestUtility::createRegexMatcher(R"raw([^.]*\.example.com)raw"));
  std::vector<SanMatcherPtr> subject_alt_name_matchers;
  subject_alt_name_matchers.push_back(
      SanMatcherPtr{std::make_unique<StringSanMatcher>(GEN_DNS, matcher)});
  EXPECT_TRUE(DefaultCertValidator::matchSubjectAltName(cert.get(), subject_alt_name_matchers));
}

TEST(DefaultCertValidatorTest, TestMatchSubjectAltNameIncorrectTypeMatched) {
  bssl::UniquePtr<X509> cert = readCertFromFile(TestEnvironment::substitute(
      "{{ test_rundir }}/test/extensions/transport_sockets/tls/test_data/san_dns_cert.pem"));
  envoy::type::matcher::v3::StringMatcher matcher;
  matcher.MergeFrom(TestUtility::createRegexMatcher(R"raw([^.]*\.example.com)raw"));
  std::vector<SanMatcherPtr> subject_alt_name_matchers;
  subject_alt_name_matchers.push_back(
      SanMatcherPtr{std::make_unique<StringSanMatcher>(GEN_URI, matcher)});
  EXPECT_FALSE(DefaultCertValidator::matchSubjectAltName(cert.get(), subject_alt_name_matchers));
}

TEST(DefaultCertValidatorTest, TestMatchSubjectAltNameWildcardDNSMatched) {
  bssl::UniquePtr<X509> cert = readCertFromFile(TestEnvironment::substitute(
      "{{ test_rundir "
      "}}/test/extensions/transport_sockets/tls/test_data/san_multiple_dns_cert.pem"));
  envoy::type::matcher::v3::StringMatcher matcher;
  matcher.set_exact("api.example.com");
  std::vector<SanMatcherPtr> subject_alt_name_matchers;
  subject_alt_name_matchers.push_back(
      SanMatcherPtr{std::make_unique<StringSanMatcher>(GEN_DNS, matcher)});
  EXPECT_TRUE(DefaultCertValidator::matchSubjectAltName(cert.get(), subject_alt_name_matchers));
}

TEST(DefaultCertValidatorTest, TestMultiLevelMatch) {
  // san_multiple_dns_cert matches *.example.com
  bssl::UniquePtr<X509> cert = readCertFromFile(TestEnvironment::substitute(
      "{{ test_rundir "
      "}}/test/extensions/transport_sockets/tls/test_data/san_multiple_dns_cert.pem"));
  envoy::type::matcher::v3::StringMatcher matcher;
  matcher.set_exact("foo.api.example.com");
  std::vector<SanMatcherPtr> subject_alt_name_matchers;
  subject_alt_name_matchers.push_back(
      SanMatcherPtr{std::make_unique<StringSanMatcher>(GEN_DNS, matcher)});
  EXPECT_FALSE(DefaultCertValidator::matchSubjectAltName(cert.get(), subject_alt_name_matchers));
}

TEST(DefaultCertValidatorTest, TestVerifySubjectAltNameURIMatched) {
  bssl::UniquePtr<X509> cert = readCertFromFile(TestEnvironment::substitute(
      "{{ test_rundir }}/test/extensions/transport_sockets/tls/test_data/san_uri_cert.pem"));
  std::vector<std::string> verify_subject_alt_name_list = {"spiffe://lyft.com/fake-team",
                                                           "spiffe://lyft.com/test-team"};
  EXPECT_TRUE(DefaultCertValidator::verifySubjectAltName(cert.get(), verify_subject_alt_name_list));
}

TEST(DefaultCertValidatorTest, TestVerifySubjectAltMultiDomain) {
  bssl::UniquePtr<X509> cert = readCertFromFile(TestEnvironment::substitute(
      "{{ test_rundir "
      "}}/test/extensions/transport_sockets/tls/test_data/san_multiple_dns_cert.pem"));
  std::vector<std::string> verify_subject_alt_name_list = {"https://a.www.example.com"};
  EXPECT_FALSE(
      DefaultCertValidator::verifySubjectAltName(cert.get(), verify_subject_alt_name_list));
}

TEST(DefaultCertValidatorTest, TestMatchSubjectAltNameURIMatched) {
  bssl::UniquePtr<X509> cert = readCertFromFile(TestEnvironment::substitute(
      "{{ test_rundir }}/test/extensions/transport_sockets/tls/test_data/san_uri_cert.pem"));
  envoy::type::matcher::v3::StringMatcher matcher;
  matcher.MergeFrom(TestUtility::createRegexMatcher(R"raw(spiffe://lyft.com/[^/]*-team)raw"));
  std::vector<SanMatcherPtr> subject_alt_name_matchers;
  subject_alt_name_matchers.push_back(
      SanMatcherPtr{std::make_unique<StringSanMatcher>(GEN_URI, matcher)});
  EXPECT_TRUE(DefaultCertValidator::matchSubjectAltName(cert.get(), subject_alt_name_matchers));
}

TEST(DefaultCertValidatorTest, TestVerifySubjectAltNameNotMatched) {
  bssl::UniquePtr<X509> cert = readCertFromFile(TestEnvironment::substitute(
      "{{ test_rundir }}/test/extensions/transport_sockets/tls/test_data/san_dns_cert.pem"));
  std::vector<std::string> verify_subject_alt_name_list = {"foo", "bar"};
  EXPECT_FALSE(
      DefaultCertValidator::verifySubjectAltName(cert.get(), verify_subject_alt_name_list));
}

TEST(DefaultCertValidatorTest, TestMatchSubjectAltNameNotMatched) {
  bssl::UniquePtr<X509> cert = readCertFromFile(TestEnvironment::substitute(
      "{{ test_rundir }}/test/extensions/transport_sockets/tls/test_data/san_dns_cert.pem"));
  envoy::type::matcher::v3::StringMatcher matcher;
  matcher.MergeFrom(TestUtility::createRegexMatcher(R"raw([^.]*\.example\.net)raw"));
  std::vector<SanMatcherPtr> subject_alt_name_matchers;
  subject_alt_name_matchers.push_back(
      SanMatcherPtr{std::make_unique<StringSanMatcher>(GEN_DNS, matcher)});
  subject_alt_name_matchers.push_back(
      SanMatcherPtr{std::make_unique<StringSanMatcher>(GEN_IPADD, matcher)});
  subject_alt_name_matchers.push_back(
      SanMatcherPtr{std::make_unique<StringSanMatcher>(GEN_URI, matcher)});
  subject_alt_name_matchers.push_back(
      SanMatcherPtr{std::make_unique<StringSanMatcher>(GEN_EMAIL, matcher)});
  EXPECT_FALSE(DefaultCertValidator::matchSubjectAltName(cert.get(), subject_alt_name_matchers));
}

TEST(DefaultCertValidatorTest, TestCertificateVerificationWithSANMatcher) {
  Stats::TestUtil::TestStore test_store;
  SslStats stats = generateSslStats(test_store);
  // Create the default validator object.
  auto default_validator =
      std::make_unique<Extensions::TransportSockets::Tls::DefaultCertValidator>(
          /*CertificateValidationContextConfig=*/nullptr, stats,
          Event::GlobalTimeSystem().timeSystem());

  bssl::UniquePtr<X509> cert = readCertFromFile(TestEnvironment::substitute(
      "{{ test_rundir }}/test/extensions/transport_sockets/tls/test_data/san_dns_cert.pem"));
  envoy::type::matcher::v3::StringMatcher matcher;
  matcher.MergeFrom(TestUtility::createRegexMatcher(R"raw([^.]*\.example.com)raw"));
  std::vector<SanMatcherPtr> san_matchers;
  san_matchers.push_back(SanMatcherPtr{std::make_unique<StringSanMatcher>(GEN_DNS, matcher)});
  // Verify the certificate with correct SAN regex matcher.
  EXPECT_EQ(default_validator->verifyCertificate(cert.get(), /*verify_san_list=*/{}, san_matchers),
            Envoy::Ssl::ClientValidationStatus::Validated);
  EXPECT_EQ(stats.fail_verify_san_.value(), 0);

  matcher.MergeFrom(TestUtility::createExactMatcher("hello.example.com"));
  std::vector<SanMatcherPtr> invalid_san_matchers;
  invalid_san_matchers.push_back(
      SanMatcherPtr{std::make_unique<StringSanMatcher>(GEN_DNS, matcher)});
  // Verify the certificate with incorrect SAN exact matcher.
  EXPECT_EQ(default_validator->verifyCertificate(cert.get(), /*verify_san_list=*/{},
                                                 invalid_san_matchers),
            Envoy::Ssl::ClientValidationStatus::Failed);
  EXPECT_EQ(stats.fail_verify_san_.value(), 1);
}

TEST(DefaultCertValidatorTest, TestCertificateVerificationWithNoValidationContext) {
  Stats::TestUtil::TestStore test_store;
  SslStats stats = generateSslStats(test_store);
  // Create the default validator object.
  auto default_validator =
      std::make_unique<Extensions::TransportSockets::Tls::DefaultCertValidator>(
          /*CertificateValidationContextConfig=*/nullptr, stats,
          Event::GlobalTimeSystem().timeSystem());

  EXPECT_EQ(default_validator->verifyCertificate(/*cert=*/nullptr, /*verify_san_list=*/{},
                                                 /*subject_alt_name_matchers=*/{}),
            Envoy::Ssl::ClientValidationStatus::NotValidated);
  bssl::UniquePtr<X509> cert(X509_new());
  EXPECT_EQ(default_validator->doVerifyCertChain(/*store_ctx=*/nullptr,
                                                 /*ssl_extended_info=*/nullptr,
                                                 /*leaf_cert=*/*cert,
                                                 /*transport_socket_options=*/nullptr),
            0);
}

TEST(DefaultCertValidatorTest, NoSanInCert) {
  bssl::UniquePtr<X509> cert = readCertFromFile(TestEnvironment::substitute(
      "{{ test_rundir }}/test/extensions/transport_sockets/tls/test_data/fake_ca_cert.pem"));
  envoy::type::matcher::v3::StringMatcher matcher;
  matcher.MergeFrom(TestUtility::createRegexMatcher(R"raw([^.]*\.example\.net)raw"));
  std::vector<SanMatcherPtr> subject_alt_name_matchers;
  subject_alt_name_matchers.push_back(
      SanMatcherPtr{std::make_unique<StringSanMatcher>(GEN_DNS, matcher)});
  EXPECT_FALSE(DefaultCertValidator::matchSubjectAltName(cert.get(), subject_alt_name_matchers));
}

TEST(DefaultCertValidatorTest, WithVerifyDepth) {

  Stats::TestUtil::TestStore test_store;
  SslStats stats = generateSslStats(test_store);
  envoy::config::core::v3::TypedExtensionConfig typed_conf;
  std::vector<envoy::extensions::transport_sockets::tls::v3::SubjectAltNameMatcher> san_matchers{};

  bssl::UniquePtr<STACK_OF(X509)> cert_chain = readCertChainFromFile(TestEnvironment::substitute(
      "{{ test_rundir "
      "}}/test/extensions/transport_sockets/tls/test_data/test_long_cert_chain.pem"));
  bssl::UniquePtr<X509> cert = readCertFromFile(TestEnvironment::substitute(
      "{{ test_rundir }}/test/extensions/transport_sockets/tls/test_data/test_random_cert.pem"));
  bssl::UniquePtr<X509> ca_cert = readCertFromFile(TestEnvironment::substitute(
      "{{ test_rundir }}/test/extensions/transport_sockets/tls/test_data/ca_cert.pem"));

  // Create the default validator object.
  // Config includes ca_cert and the verify-depth
  // Set verify depth < 3, so verification fails. ( There are 3 intermediate certs )

  std::string ca_cert_str(TestEnvironment::substitute(
      "{{ test_rundir }}/test/extensions/transport_sockets/tls/test_data/ca_cert.pem"));
  TestCertificateValidationContextConfigPtr test_config =
      std::make_unique<TestCertificateValidationContextConfig>(typed_conf, false, san_matchers,
                                                               ca_cert_str, 2);
  auto default_validator =
      std::make_unique<Extensions::TransportSockets::Tls::DefaultCertValidator>(
          test_config.get(), stats, Event::GlobalTimeSystem().timeSystem());

  STACK_OF(X509)* intermediates = cert_chain.get();
  SSLContextPtr ssl_ctx = SSL_CTX_new(TLS_method());
  X509StoreContextPtr store_ctx = X509_STORE_CTX_new();

  X509_STORE* storep = SSL_CTX_get_cert_store(ssl_ctx.get());
  X509_STORE_add_cert(storep, ca_cert.get());
  EXPECT_TRUE(X509_STORE_CTX_init(store_ctx.get(), storep, cert.get(), intermediates));

  default_validator->addClientValidationContext(ssl_ctx.get(), false);
  X509_VERIFY_PARAM_set1(X509_STORE_CTX_get0_param(store_ctx.get()),
                         SSL_CTX_get0_param(ssl_ctx.get()));

  EXPECT_EQ(X509_verify_cert(store_ctx.get()), 0);

  // Now, create config with no depth configuration, verification should pass
  test_config = std::make_unique<TestCertificateValidationContextConfig>(typed_conf, false,
                                                                         san_matchers, ca_cert_str);
  default_validator = std::make_unique<Extensions::TransportSockets::Tls::DefaultCertValidator>(
      test_config.get(), stats, Event::GlobalTimeSystem().timeSystem());

  // Re-initialize context
  ssl_ctx = SSL_CTX_new(TLS_method());
  storep = SSL_CTX_get_cert_store(ssl_ctx.get());
  X509_STORE_add_cert(storep, ca_cert.get());
  EXPECT_TRUE(X509_STORE_CTX_init(store_ctx.get(), storep, cert.get(), intermediates));

  default_validator->addClientValidationContext(ssl_ctx.get(), false);
  X509_VERIFY_PARAM_set1(X509_STORE_CTX_get0_param(store_ctx.get()),
                         SSL_CTX_get0_param(ssl_ctx.get()));

  EXPECT_EQ(X509_verify_cert(store_ctx.get()), 1);
  EXPECT_EQ(X509_STORE_CTX_get_error(store_ctx.get()), X509_V_OK);
}

} // namespace Tls
} // namespace TransportSockets
} // namespace Extensions
} // namespace Envoy
