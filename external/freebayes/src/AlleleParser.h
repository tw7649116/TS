#ifndef _ALLELE_PARSER_H
#define _ALLELE_PARSER_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <deque>
#include <utility>
#include <algorithm>
#include <time.h>
#include <assert.h>
#include <ctype.h>
#include <cmath>
#include "split.h"
#include "join.h"
#include "api/BamReader.h"
#include "BedReader.h"
#include "Utility.h"
#include "Allele.h"
#include "Sample.h"
#include "Fasta.h"
#include "Parameters.h"
#include "TryCatch.h"
#include "api/BamMultiReader.h"
#include "Genotype.h"
#include "CNV.h"
#include "Result.h"
#include "LeftAlign.h"
//#include "../vcflib/Variant.h"
#include "Version.h"
#include <Variant.h>
#include <smithwaterman/SmithWatermanGotoh.h>

// the size of the window of the reference which is always cached in memory
#define CACHED_REFERENCE_WINDOW 300

// the window of haplotype basis alleles which we ensure we keep
// increasing this reduces disk access when using haplotype basis alleles, but increases memory usage
#define CACHED_BASIS_HAPLOTYPE_WINDOW 1000

using namespace std;
using namespace BamTools;

// a structure holding information about our parameters

// structure to encapsulate registered reads and alleles
class RegisteredAlignment {
    friend ostream &operator<<(ostream &out, RegisteredAlignment &a);
public:
    //BamAlignment alignment;
    long unsigned int start;
    long unsigned int end;
    int refid;
    string name;
    vector<Allele> alleles;
    int mismatches;
    int snpCount;
    int indelCount;
    int alleleTypes;

    RegisteredAlignment(BamAlignment& alignment)
        //: alignment(alignment)
        : start(alignment.Position)
        , end(alignment.GetEndPosition())
        , refid(alignment.RefID)
        , name(alignment.Name)
        , mismatches(0)
        , snpCount(0)
        , indelCount(0)
        , alleleTypes(0)
    { }

    void addAllele(Allele allele, bool mergeComplex = true, int maxComplexGap = 0);
    bool fitHaplotype(int pos, int haplotypeLength, Allele*& aptr);

};

// functor to filter alleles outside of our analysis window
class AlleleFilter {

public:
    AlleleFilter(long unsigned int s, long unsigned int e) : start(s), end(e) {}

    // true of the allele is outside of our window
    bool operator()(Allele& a) { 
        return !(((long int)start) >= a.position && ((long int)end) < a.position + a.length);
    }

    bool operator()(Allele*& a) { 
        return !(((long int)start) >= a->position && ((long int)end) < a->position + a->length);
    }

private:
    long unsigned int start, end;

};

class AllelePtrCmp {

public:
    bool operator()(Allele* &a, Allele* &b) {
        return a->type < b->type;
    }

};

class AllelicPrimitive {
public:
    
    string ref;
    string alt;
    AllelicPrimitive(string& r, string& a)
	: ref(r)
	, alt(a) { }
};

bool operator<(const AllelicPrimitive& a, const AllelicPrimitive& b);

class AlleleParser {

public:

    Parameters parameters; // holds operational parameters passed at program invocation
    
    //AlleleParser(int argc, char** argv);
    //AlleleParser();
     AlleleParser(Parameters  parameters);
    ~AlleleParser(void); 

    vector<string> sampleList; // list of sample names, indexed by sample id
    vector<string> sampleListFromBam; // sample names drawn from BAM file
    vector<string> sampleListFromVCF; // sample names drawn from input VCF
    map<string, string> samplePopulation; // population subdivisions of samples
    map<string, vector<string> > populationSamples; // inversion of samplePopulation
    map<string, string> readGroupToSampleNames; // maps read groups to samples
    map<string, string> readGroupToTechnology; // maps read groups to technologies
    vector<string> sequencingTechnologies;  // a list of the present technologies

    CNVMap sampleCNV;

    // reference
    FastaReference reference;
    vector<string> referenceSequenceNames;
    map<int, string> referenceIDToName;
    
    // target regions
    vector<BedTarget> targets;
    // returns true if we are within a target
    // useful for controlling output when we are reading from stdin
    bool inTarget(void);

    // bamreader
    BamMultiReader bamMultiReader;

    // bed reader
    BedReader bedReader;

    // VCF
    vcf::VariantCallFile variantCallFile;
    vcf::VariantCallFile variantCallInputFile;   // input variant alleles, to target analysis
    vcf::VariantCallFile haplotypeVariantInputFile;  // input alleles which will be used to construct haplotype alleles

    // input haplotype alleles
    // 
    // as calling progresses, a window of haplotype basis alleles from the flanking sequence
    // map from starting position to length->alle
    map<long int, vector<AllelicPrimitive> > haplotypeBasisAlleles;  // this is in the current reference sequence
    bool usingHaplotypeBasisAlleles;
    long int rightmostHaplotypeBasisAllelePosition;
    void updateHaplotypeBasisAlleles(long int pos, int referenceLength);
    bool allowedAllele(long int pos, string& ref, string& alt);

    Allele makeAllele(RegisteredAlignment& ra,
		      AlleleType type,
		      long int pos,
		      int length,
		      int basesLeft,
		      int basesRight,
		      string& readSequence,
		      string& sampleName,
		      BamAlignment& alignment,
		      string& sequencingTech,
		      long double qual,
		      string& qualstr);



    vector<Allele*> registeredAlleles;
    map<long unsigned int, deque<RegisteredAlignment> > registeredAlignments;
    map<long int, vector<Allele> > inputVariantAlleles; // all variants present in the input VCF, as 'genotype' alleles
    //  position         sample     genotype  likelihood
    map<long int, map<string, map<string, long double> > > inputGenotypeLikelihoods; // drawn from input VCF
    map<long int, map<Allele, int> > inputAlleleCounts; // drawn from input VCF
    Sample* nullSample;
    vector<vcf::Variant> inputVariantsWithinHaploBases;
    
    void addCurrentGenotypeLikelihoods(map<int, vector<Genotype> >& genotypesByPloidy,
            vector<vector<SampleDataLikelihood> >& sampleDataLikelihoods);

    void getInputAlleleCounts(vector<Allele>& genotypeAlleles, map<string, int>& inputAFs);

    // reference names indexed by id
    vector<RefData> referenceSequences;
    // ^^ vector of objects containing:
    //RefName;          //!< Name of reference sequence
    //RefLength;        //!< Length of reference sequence
    //RefHasAlignments; //!< True if BAM file contains alignments mapped to reference sequence

    string bamHeader;
    vector<string> bamHeaderLines;
 
    void openBams(void);
    void openTraceFile(void);
    void openFailedFile(void);
    void openOutputFile(void);
    void getSampleNames(void);
    void getPopulations(void);
    void getSequencingTechnologies(void);
    void loadSampleCNVMap(void);
    int currentSamplePloidy(string const& sample);
    vector<int> currentPloidies(Samples& samples);
    void loadBamReferenceSequenceNames(void);
    void loadFastaReference(void);
    void loadReferenceSequence(BedTarget*, int, int);
    void loadReferenceSequence(BamAlignment& alignment);
    void preserveReferenceSequenceWindow(int bp);
    void extendReferenceSequence(int);
    void extendReferenceSequence(BamAlignment& alignment);
    void eraseReferenceSequence(int leftErasure);
    string referenceSubstr(long int position, unsigned int length);
    void loadTargets(void);
    bool getFirstAlignment(void);
    bool getFirstVariant(void);
    void loadTargetsFromBams(void);
    void initializeOutputFiles(void);
    RegisteredAlignment& registerAlignment(BamAlignment& alignment, RegisteredAlignment& ra, string& sampleName, string& sequencingTech);
    void clearRegisteredAlignments(void);
    void updateAlignmentQueue(void);
    void updateInputVariants(bool);
    void updateInputVariant(void);
    void updateHaplotypeBasisAlleles(void);
    void removeNonOverlappingAlleles(vector<Allele*>& alleles, int haplotypeLength = 1, bool getAllAllelesInHaplotype = false);
    void removeFilteredAlleles(vector<Allele*>& alleles);
    void updateRegisteredAlleles(void);
    void updatePriorAlleles(void);
    vector<BedTarget>* targetsInCurrentRefSeq(void);
    bool toNextRefID(void);
    bool loadTarget(BedTarget*);
    bool toFirstTargetPosition(void);
    bool toNextPosition(bool &, bool);
    bool dummyProcessNextTarget(void);
    bool toNextTarget(void);
    void setPosition(long unsigned int);
    int currentSequencePosition(const BamAlignment& alignment);
    int currentSequencePosition();
    bool getNextAlleles(Samples& allelesBySample, int allowedAlleleTypes);
    // builds up haplotype (longer, e.g. ref+snp+ref) alleles to match the longest allele in genotypeAlleles
    // updates vector<Allele>& alleles with the new alleles
    void buildHaplotypeAlleles(vector<Allele>& alleles, Samples& allelesBySample, map<string, vector<Allele*> >& alleleGroups, int allowedAlleleTypes);
    void getAlleles(Samples& allelesBySample, int allowedAlleleTypes, int haplotypeLength = 1, bool getAllAllelesInHaplotype = false);
    Allele* referenceAllele(int mapQ, int baseQ);
    Allele* alternateAllele(int mapQ, int baseQ);
    int homopolymerRunLeft(string altbase);
    int homopolymerRunRight(string altbase);
    map<string, int> repeatCounts(long int position, const string& sequence, int maxsize);
    map<long int, map<string, int> > cachedRepeatCounts; // cached version of previous
    bool isRepeatUnit(const string& seq, const string& unit);
    void setupVCFOutput(void);
    void setupVCFInput(void);
    string vcfHeader(void);
    bool hasInputVariantAllelesAtCurrentPosition(void);
    bool mergeVariants(string contigName_first_variant, long int position_first_variant, string ref_first_variant, vector<string> alt_first_variant,
			string contigName_second_variant, long int position_second_variant, string ref_second_variant, vector<string> alt_second_variant,
			long int & merged_start_position, string & merged_ref_allele, vector<string> & merged_alt_allele );
    
    // gets the genotype alleles we should evaluate among the allele groups and
    // sample groups at the current position, according to our filters
    vector<Allele> genotypeAlleles(map<string, vector<Allele*> >& alleleGroups,
            Samples& samples,
            bool useOnlyInputAlleles);

    // pointer to current position in targets
    int fastaReferenceSequenceCount; // number of reference sequences
    bool hasTarget;
    BedTarget* currentTarget;
    long int currentPosition;  // 0-based current position
    int lastHaplotypeLength;
    char currentReferenceBase;
    string currentSequence;
    char currentReferenceBaseChar();
    string currentReferenceBaseString();
    string::iterator currentReferenceBaseIterator();

    // output files
    ofstream logFile, outputFile, traceFile, failedFile;
    ostream* output;

    // utility
    bool isCpG(string& altbase);

    string currentSequenceName;

private:

    bool justSwitchedTargets;  // to trigger clearing of queues, maps and such holding Allele*'s on jump

    Allele* currentReferenceAllele;
    Allele* currentAlternateAllele;

    //BedTarget currentSequenceBounds;
    long int currentSequenceStart;

    bool hasMoreAlignments;
    bool hasMoreVariants;;

    bool oneSampleAnalysis; // if we are analyzing just one sample, and there are no specified read groups

    int basesBeforeCurrentTarget; // number of bases in sequence we're storing before the current target
    int basesAfterCurrentTarget;  // ........................................  after ...................

    int currentRefID;
    BamAlignment currentAlignment;
    vcf::Variant* currentVariant;
    vcf::Variant* previousVariant;

};

#endif
