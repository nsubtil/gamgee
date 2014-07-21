#include "variant.h"
#include "variant_reader.h"
#include "multiple_variant_reader.h"
#include "multiple_variant_iterator.h"
#include "is_missing.h"

#include <boost/test/unit_test.hpp>

using namespace std;
using namespace gamgee;

constexpr auto FLOAT_COMPARISON_THRESHOLD = 0.0001f;

const auto truth_chromosome        = vector<uint32_t>{0, 1, 1, 1, 2};
const auto truth_alignment_starts  = vector<uint32_t>{10000000, 10001000, 10002000, 10003000, 10004000};
const auto truth_n_alleles         = vector<uint32_t>{2, 2, 2, 2, 3};
const auto truth_filter_name       = vector<string>{"PASS", "PASS", "LOW_QUAL", "NOT_DEFINED", "PASS"};
const auto truth_filter_size       = vector<uint32_t>{1,1,1,1,2};
const auto truth_quals             = vector<float>{80,8.4,-1,-1,-1};
const auto truth_ref               = vector<string>{"T", "GG", "TAGTGQA", "A", "GAT"};
const auto truth_alt               = vector< vector<string>> {  { "C" } , {"AA"},  {"T"},  {"AGCT"},  {"G","GATAT"}};
const auto truth_high_quality_hets = boost::dynamic_bitset<>{std::string{"001"}};
const auto truth_id                = vector<string>{"db2342", "rs837472", ".", ".", "."};
const auto truth_gq                = vector<vector<uint32_t>>{{25,12,650}, {35,35,35}, {35,35,35}, {35,35,35}, {35,35,35}};
const auto truth_af                = vector<float> { 3.1,2.2 };
const auto truth_pl                = vector<vector<vector<uint32_t>>>{
  {{10,0,100      }, {0,10,1000      }, {10,100,0}      },
  {{10,0,100      }, {0,10,100       }, {10,100,0}      },
  {{10,0,100      }, {0,10,2000000000}, {10,100,0}      },
  {{10,0,100      }, {0,10,100       }, {10,100,0}      },
  {{10,0,100,2,4,8}, {0,10,100,2,4,8 }, {10,100,0,2,4,8}}
};
const auto truth_as                = vector<vector<string>>{ 
  {"ABA","CA","XISPAFORRO"}, 
  {"ABA","ABA","ABA"}, 
  {"ABA","ABA","ABA"}, 
  {"ABA","ABA","ABA"}, 
  {"ABA","ABA","ABA"} 
};




boost::dynamic_bitset<> high_qual_hets(const Variant& record) {  // filter all hets that have GQ > 20
  const auto genotypes = record.genotypes(); // a "vector-like" with the genotypes of all samples in this record
  const auto gqs = record.genotype_quals(); // a "vector-like" with all the GQs of all samples in this record
  const auto hets = Variant::select_if<Genotype>(genotypes.begin(), genotypes.end(), [](const auto& g) { return g.is_het(); }); // returns a bit set with all hets marked with 1's
  const auto pass_gqs = Variant::select_if<VariantFieldValue<int32_t>>(gqs.begin(), gqs.end(), [](const auto& gq) { return gq[0] > 20; }); // returns a bit set with every sample with gq > 20 marked with 1's
  return hets & pass_gqs; // returns a bit set with all the samples that are het and have gq > 20
}

void check_variant_basic_api(const Variant& record, const uint32_t truth_index) {
  BOOST_CHECK_EQUAL(record.ref(), truth_ref[truth_index]);
  BOOST_CHECK_EQUAL(record.chromosome(), truth_chromosome[truth_index]);
  BOOST_CHECK_EQUAL(record.alignment_start(), truth_alignment_starts[truth_index]);
  BOOST_CHECK_EQUAL(record.n_alleles(), truth_n_alleles[truth_index]);
  BOOST_CHECK_EQUAL(record.n_samples(), 3);
  BOOST_CHECK_EQUAL(record.id(), truth_id[truth_index]);
}


void check_quals_api(const Variant& record, const uint32_t truth_index) {
  if (truth_quals[truth_index] < 0) 
    BOOST_CHECK(is_missing(record.qual()));
  else 
    BOOST_CHECK_EQUAL(record.qual(), truth_quals[truth_index]); 
}

void check_alt_api(const Variant& record, const uint32_t truth_index) {
  auto alt = record.alt();
  BOOST_CHECK_EQUAL_COLLECTIONS(alt.begin(), alt.end(), truth_alt[truth_index].begin(), truth_alt[truth_index].end());
}

void check_filters_api(const Variant& record, const uint32_t truth_index) {
  BOOST_CHECK(record.has_filter(truth_filter_name[truth_index])); // check the has_filter member function
  const auto low_qual_filter_present = record.has_filter("LOW_QUAL");
  if (truth_index == 2)
    BOOST_CHECK(low_qual_filter_present == true);
  else
    BOOST_CHECK(low_qual_filter_present == false);
  const auto filters = record.filters(); 
  BOOST_CHECK_EQUAL(filters.size(), truth_filter_size[truth_index]); // checking size member function
  BOOST_CHECK_EQUAL(filters[0], truth_filter_name[truth_index]);  // checking random access
  BOOST_CHECK_EQUAL(count_if(filters.begin(), filters.end(), [](const auto x){return x == x;}), truth_filter_size[truth_index]); // checking iteration in functional style
}

void check_genotype_quals_api(const Variant& record, const uint32_t truth_index) {
  const auto gqs = record.genotype_quals();
  // direct access
  for (auto i = 0u; i != record.n_samples(); ++i) 
    BOOST_CHECK_EQUAL(gqs[i][0], truth_gq[truth_index][i]);
  // functional style access
  auto i = 0;
  for_each(gqs.begin(), gqs.end(), [&truth_index, &i](const VariantFieldValue<int32_t>& x) { BOOST_CHECK_EQUAL(x[0], truth_gq[truth_index][i++]); }); // testing std library functional call at the samples level
}

void check_phred_likelihoods_api(const Variant& record, const uint32_t truth_index) {
  const auto pls = record.phred_likelihoods();
  // direct access
  for(auto i = 0u; i != record.n_samples(); ++i) 
    for (auto j = 0u; j != pls[i].size(); ++j) 
      BOOST_CHECK_EQUAL(pls[i][j], truth_pl[truth_index][i][j]);
  // functional style access
  for(const auto& sample_pl : pls) { // testing for-each iteration at the samples level
    BOOST_CHECK_EQUAL(1, count_if(sample_pl.begin(), sample_pl.end(), [](const auto& x) { return x == 0; })); // testing std library functional call at the sample value level
    for(const auto& value_pl : sample_pl)  // testing for-each iteration at the sample value level
      BOOST_CHECK_EQUAL(value_pl, value_pl); // I just want to make sure that this level of iteration is supported, the values don't matter anymore at this point
  }
}      

void check_format_field_api(const Variant& record, const uint32_t truth_index) {
  const auto gq_int    = record.generic_integer_format_field("GQ");
  const auto gq_float  = record.generic_float_format_field("GQ");
  const auto gq_string = record.generic_string_format_field("GQ");
  const auto af_int    = record.generic_integer_format_field("AF");
  const auto af_float  = record.generic_float_format_field("AF");
  const auto af_string = record.generic_string_format_field("AF");
  const auto pl_int    = record.generic_integer_format_field("PL");
  const auto pl_float  = record.generic_float_format_field("PL");
  const auto pl_string = record.generic_string_format_field("PL");
  const auto as_int    = record.generic_integer_format_field("AS"); // this is a string field, we should be able to create the object but not access it's elements due to lazy initialization
  const auto as_float  = record.generic_float_format_field("AS");   // this is a string field, we should be able to create the object but not access it's elements due to lazy initialization
  const auto as_string = record.generic_string_format_field("AS");
  for(auto i=0u; i != record.n_samples(); ++i) {
    BOOST_CHECK_EQUAL(gq_int[i][0], truth_gq[truth_index][i]);
    BOOST_CHECK_CLOSE(gq_float[i][0], float(truth_gq[truth_index][i]), FLOAT_COMPARISON_THRESHOLD);
    BOOST_CHECK_EQUAL(gq_string[i][0], to_string(truth_gq[truth_index][i]));
    BOOST_REQUIRE_EQUAL(af_int[i].size(), truth_af.size()); // require otherwise next line may segfault
    for (auto j=0u; j!= af_int[i].size(); ++j) {
      BOOST_CHECK_EQUAL(af_int[i][j], int32_t(truth_af[j]));
      BOOST_CHECK_CLOSE(af_float[i][j], truth_af[j], FLOAT_COMPARISON_THRESHOLD);
      BOOST_CHECK_EQUAL(af_string[i][j], to_string(truth_af[j]));
    }
    BOOST_REQUIRE_EQUAL(pl_int[i].size(), truth_pl[truth_index][i].size()); // require otherwise next line may segfault
    for (auto j=0u; j!= pl_int[i].size(); ++j) { 
      BOOST_CHECK_EQUAL(pl_int[i][j], truth_pl[truth_index][i][j]);
      BOOST_CHECK_CLOSE(pl_float[i][j], float(truth_pl[truth_index][i][j]), FLOAT_COMPARISON_THRESHOLD);
      BOOST_CHECK_EQUAL(pl_string[i][j], to_string(truth_pl[truth_index][i][j]));
    }
    BOOST_CHECK_EQUAL(as_string[i][0], truth_as[truth_index][i]);
  }
  BOOST_CHECK_THROW(as_float[0][0], invalid_argument); 
  BOOST_CHECK_THROW(as_int[0][0], invalid_argument); 
}

void check_info_field_api(const Variant& record, const uint32_t truth_index) {
  const auto validated_actual = record.generic_boolean_info_field("VALIDATED");
  const auto validated_expected = std::vector<bool>{truth_index == 0};
  BOOST_CHECK_EQUAL_COLLECTIONS(validated_actual.begin(), validated_actual.end(), validated_expected.begin(), validated_expected.end());
  const auto an = record.generic_integer_info_field("AN");
  BOOST_CHECK_EQUAL(an.size(), 1);
  BOOST_CHECK_EQUAL(an[0], 6);
  const auto af_actual = record.generic_float_info_field("AF");
  const auto af_expected = truth_index == 4 ? std::vector<float>{0.5, 0} : std::vector<float>{0.5};
  BOOST_CHECK_EQUAL_COLLECTIONS(af_actual.begin(), af_actual.end(), af_expected.begin(), af_expected.end());
  const auto desc_actual = record.generic_string_info_field("DESC");
  const auto desc_expected = truth_index == 0 ? std::vector<string>{"Test1,Test2"} : std::vector<string>{};
  BOOST_CHECK_EQUAL_COLLECTIONS(desc_actual.begin(), desc_actual.end(), desc_expected.begin(), desc_expected.end());
}

void check_genotype_api(const Variant& record, const uint32_t truth_index) {
  BOOST_CHECK_EQUAL(high_qual_hets(record), truth_high_quality_hets);
  const auto gt_for_all_samples = record.genotypes();
  BOOST_CHECK(gt_for_all_samples[1].is_hom_ref());
  BOOST_CHECK(gt_for_all_samples[0].is_het());
  BOOST_CHECK(gt_for_all_samples[2].is_hom_var());
  for (const auto& gt_for_single_sample: gt_for_all_samples) {
    BOOST_CHECK_EQUAL(gt_for_single_sample.size(), 2);
    const auto alleles = gt_for_single_sample.alleles_strings();
    const auto allele_keys = gt_for_single_sample.alleles_keys();
    BOOST_CHECK_EQUAL(gt_for_single_sample.allele_string(0), alleles[0]);
    BOOST_CHECK_EQUAL(gt_for_single_sample.allele_string(1), alleles[1]);
    BOOST_CHECK_EQUAL(gt_for_single_sample.allele_key(0), allele_keys[0]);
    BOOST_CHECK_EQUAL(gt_for_single_sample.allele_key(1), allele_keys[1]);
    BOOST_CHECK_EQUAL(gt_for_single_sample[0], allele_keys[0]);
    BOOST_CHECK_EQUAL(gt_for_single_sample[1], allele_keys[1]);
    BOOST_CHECK(!is_missing(gt_for_single_sample));
    BOOST_CHECK_EQUAL(gt_for_single_sample.size(), 2);
    if (gt_for_single_sample.is_het()) {
      BOOST_CHECK(!gt_for_single_sample.is_hom_ref());
      BOOST_CHECK(!gt_for_single_sample.is_hom_var());
      BOOST_CHECK_NE(allele_keys[0], allele_keys[1]);
      BOOST_CHECK_EQUAL(allele_keys[0], 0);
      BOOST_CHECK_EQUAL(allele_keys[1], 1);
      BOOST_CHECK_NE(alleles[0], alleles[1]);
      BOOST_CHECK_EQUAL(alleles[0], truth_ref[truth_index]);
      BOOST_CHECK_EQUAL(alleles[1], truth_alt[truth_index][0]);
      BOOST_CHECK_EQUAL(gt_for_single_sample.fast_diploid_key_generation(), 0x00000001);
    }
    if (gt_for_single_sample.is_hom_ref()) {
      BOOST_CHECK(!gt_for_single_sample.is_het());
      BOOST_CHECK(!gt_for_single_sample.is_hom_var());
      BOOST_CHECK_EQUAL(allele_keys[0], allele_keys[1]);
      BOOST_CHECK_EQUAL(allele_keys[0], 0);
      BOOST_CHECK_EQUAL(allele_keys[1], 0);
      BOOST_CHECK_EQUAL(alleles[0], alleles[1]);
      BOOST_CHECK_EQUAL(alleles[0], truth_ref[truth_index]);
      BOOST_CHECK_EQUAL(alleles[1], truth_ref[truth_index]);
      BOOST_CHECK_EQUAL(gt_for_single_sample.fast_diploid_key_generation(), 0x00000000);
    }
    if (gt_for_single_sample.is_hom_var()) {
      BOOST_CHECK(!gt_for_single_sample.is_het());
      BOOST_CHECK(!gt_for_single_sample.is_hom_ref());
      BOOST_CHECK_EQUAL(allele_keys[0], allele_keys[1]);
      BOOST_CHECK_EQUAL(allele_keys[0], 1);
      BOOST_CHECK_EQUAL(allele_keys[1], 1);
      BOOST_CHECK_EQUAL(alleles[0], alleles[1]);
      BOOST_CHECK_EQUAL(alleles[0], truth_alt[truth_index][0]);
      BOOST_CHECK_EQUAL(alleles[1], truth_alt[truth_index][0]);
      BOOST_CHECK_EQUAL(gt_for_single_sample.fast_diploid_key_generation(), 0x00010001);
    }
    if (truth_index == 0) {
      BOOST_CHECK(gt_for_all_samples[0] == gt_for_all_samples[0]);
      BOOST_CHECK(!(gt_for_all_samples[0] != gt_for_all_samples[0]));
      BOOST_CHECK(gt_for_all_samples[0] != gt_for_all_samples[1]);
      BOOST_CHECK(!(gt_for_all_samples[0] == gt_for_all_samples[1]));
      BOOST_CHECK(gt_for_all_samples[0][1] == gt_for_all_samples[2][0]);
      BOOST_CHECK(!(gt_for_all_samples[0][1] != gt_for_all_samples[2][0]));
      BOOST_CHECK(gt_for_all_samples[0][1] == gt_for_all_samples[0][1]);
      BOOST_CHECK(!(gt_for_all_samples[0][1] != gt_for_all_samples[0][1]));
      BOOST_CHECK(gt_for_all_samples[0][1] != gt_for_all_samples[1][0]);
      BOOST_CHECK(!(gt_for_all_samples[0][1] == gt_for_all_samples[1][0]));
    }
  }
}

BOOST_AUTO_TEST_CASE( single_variant_reader ) 
{
  for (const auto& filename : {"testdata/test_variants.vcf", "testdata/test_variants.bcf"}) {
    auto truth_index = 0u;
    for (const auto& record : SingleVariantReader{filename}) {
      check_variant_basic_api(record, truth_index);
      check_quals_api(record, truth_index);
      check_alt_api(record, truth_index);
      check_filters_api(record, truth_index);
      check_genotype_quals_api(record,truth_index);
      check_phred_likelihoods_api(record, truth_index);
      check_format_field_api(record, truth_index);
      check_info_field_api(record, truth_index);
      check_genotype_api(record, truth_index);
      ++truth_index;
    }
    BOOST_CHECK_EQUAL(truth_index, 5u);
  }
}

BOOST_AUTO_TEST_CASE( single_variant_reader_sites_only )  
{
  for (const auto& record : SingleVariantReader{"testdata/test_variants.vcf", vector<string>{}})  // exclude all samples (sites-only)
    BOOST_CHECK_EQUAL(record.n_samples(), 0);
}

BOOST_AUTO_TEST_CASE( missing_id_field )  
{
  const auto truth_missing = vector<bool>{false, false, true, true, true};
  auto i = 0u;
  for (const auto& record : SingleVariantReader{"testdata/test_variants.vcf"}) 
    BOOST_CHECK_EQUAL(is_missing(record.id()), truth_missing[i++]); // check that the missing and non-missing values in the vcf actually return the right missingness
  BOOST_CHECK(is_missing(""));                                      // check that an empty string is missing
  BOOST_CHECK(!is_missing("arBItrary_vaLue"));                      // check that a non-empty string is not missing
}


BOOST_AUTO_TEST_CASE( single_variant_reader_include_all_samples ) 
{
  for (const auto& record : SingleVariantReader{"testdata/test_variants.vcf", vector<string>{}, false})  // include all samples by setting include == false and passing an empty list
    BOOST_CHECK_EQUAL(record.n_samples(), 3);
}

BOOST_AUTO_TEST_CASE( single_variant_reader_including )  
{
  for (const auto& record : SingleVariantReader{"testdata/test_variants.vcf", vector<string>{"NA12878"}}) // include only NA12878
    BOOST_CHECK_EQUAL(record.n_samples(), 1);
  for (const auto& record : SingleVariantReader{"testdata/test_variants.vcf", vector<string>{"NA12878", "NA12892"}}) // include both these samples
    BOOST_CHECK_EQUAL(record.n_samples(), 2);
}

BOOST_AUTO_TEST_CASE( single_variant_reader_excluding )  
{
  for (const auto& record : SingleVariantReader{"testdata/test_variants.vcf", vector<string>{"NA12891"}, false})  // exclude only NA12891
    BOOST_CHECK_EQUAL(record.n_samples(), 2);
  for (const auto& record : SingleVariantReader{"testdata/test_variants.vcf", vector<string>{"NA12891", "NA12878"}, false})  // exclude both these samples
    BOOST_CHECK_EQUAL(record.n_samples(), 1);
}

BOOST_AUTO_TEST_CASE( single_variant_reader_missing_data )
{
  for (const auto& record : SingleVariantReader{"testdata/test_variants_missing_data.vcf", vector<string>{"0001A","0002A","0003A","0004A","0005A","0006A","0007A"}}){ // include 7 samples
    BOOST_CHECK_EQUAL(record.n_samples(), 7); 
  }
  for (const auto& record : SingleVariantReader{"testdata/test_variants_missing_data.vcf", vector<string>{"0007B", "0008A", "0009A", "0009B"}, false}){ // exclude 4 samples
    BOOST_CHECK_EQUAL(record.n_samples(), 7); 
  }
  for (const auto& record : SingleVariantReader{"testdata/test_variants_missing_data.vcf"}){ // include all 11 samples
    BOOST_CHECK_EQUAL(record.n_samples(), 11); 
    const auto gt_for_all_samples = record.genotypes();
    for (const auto& gt_for_single_sample: gt_for_all_samples) {
      BOOST_CHECK(is_missing(gt_for_single_sample));
      BOOST_CHECK_EQUAL(gt_for_single_sample.size(), 2);
      BOOST_CHECK(is_missing(gt_for_single_sample.allele_string(0)));
      BOOST_CHECK(is_missing(gt_for_single_sample.allele_string(1)));
      BOOST_CHECK(is_missing(gt_for_single_sample.allele_key(0)));
      BOOST_CHECK(is_missing(gt_for_single_sample.allele_key(1)));
      BOOST_CHECK(is_missing(gt_for_single_sample[0]));
      BOOST_CHECK(is_missing(gt_for_single_sample[1]));
    }
  }
}

BOOST_AUTO_TEST_CASE( multi_variant_reader_validation )
{
  const std::vector<std::string> filenames1{"testdata/test_variants.vcf", "testdata/extra_header.vcf"};
  const std::vector<std::string> filenames2{"testdata/test_variants.vcf", "testdata/missing_header.vcf"};

  // validate mismatched headers by default
  for (const auto filenames_v : {filenames1, filenames2})
    BOOST_CHECK_THROW(
      auto reader = MultipleVariantReader<MultipleVariantIterator>(filenames_v),
      std::runtime_error
      // this does not work for some reason
      // MultipleVariantReader::HeaderException
    );

  // don't validate mismatched headers
  for (const auto filenames_v : {filenames1, filenames2})
    auto reader = MultipleVariantReader<MultipleVariantIterator>(filenames_v, false);
}

BOOST_AUTO_TEST_CASE( mutliple_variant_reader_test ) {
  auto truth_index = 0u;
  const auto reader = MultipleVariantReader<MultipleVariantIterator>{{"testdata/test_variants.vcf", "testdata/test_variants.bcf"}, false};
  for (const auto& vec : reader) {
    for (const auto& record : vec) {
      check_variant_basic_api(record, truth_index);
      check_quals_api(record, truth_index);
      check_alt_api(record, truth_index);
      check_filters_api(record, truth_index);
      check_genotype_quals_api(record,truth_index);
      check_phred_likelihoods_api(record, truth_index);
      check_format_field_api(record, truth_index);
      check_info_field_api(record, truth_index);
      check_genotype_api(record, truth_index);
    }
    ++truth_index;
  }
}