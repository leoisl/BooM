#ifndef __BOO_MAP__
#define __BOO_MAP__

#include <gatb/gatb_core.hpp>

#include <fstream>
#include <vector>
#include <iterator>
#include <type_traits>

#include <sys/stat.h>

// adapted from :
// http://stackoverflow.com/questions/34875315/implementation-my-own-list-and-iterator-stl-c
template <typename Iter>
class KeyIterator {
public:
    typedef KeyIterator<Iter> self_type;
    typedef typename std::iterator_traits<Iter>::value_type::first_type value_type;
    typedef value_type& reference;
    typedef value_type* pointer;
    typedef std::forward_iterator_tag iterator_category;
    typedef int64_t difference_type;

    KeyIterator(Iter first) : curr_(first) {}
    KeyIterator operator++() { KeyIterator i = *this; curr_++; return i; }
    KeyIterator operator++(int) { ++curr_; return *this; }
    reference operator*() { return curr_->first; }
    pointer operator->() { return &(curr_->first); }
    bool operator==(const self_type& rhs) { return curr_ == rhs.curr_; }
    bool operator!=(const self_type& rhs) { return curr_ != rhs.curr_; }
    bool operator<(const self_type& rhs) { return curr_ < rhs.curr_; }
    bool operator<=(const self_type& rhs) { return curr_ <= rhs.curr_; }
    
private:
    Iter curr_;
};

template <typename KeyT, typename ValueT>
class BooMap {
public:
    using HasherT = boomphf::SingleHashFunctor<KeyT>;
    using BooPHFT = boomphf::mphf<KeyT, HasherT>;
    using IteratorT = typename std::vector<std::pair<KeyT, ValueT>>::iterator;

    BooMap() : built_(false) {}
    void add(KeyT&& k, ValueT&& v) {
        data_.emplace_back(k, v);
    }
    void add(const KeyT& k, const ValueT& v) {
        data_.emplace_back(k, v);
    }

    bool validate_hash(){
        for( auto& e : data_ ) {
            if (e.first != data_[boophf_->lookup(e.first)].first) {
                std::cerr << "lookup of " << e.first << " failed!\n";
            }
        }
        return true;
    }

    bool build(int nthreads=1) {
        size_t numElem = data_.size();
        KeyIterator<decltype(data_.begin())> kb(data_.begin());
        KeyIterator<decltype(data_.begin())> ke(data_.end());
        auto keyIt = boomphf::range(kb, ke);
        BooPHFT* ph = new BooPHFT(numElem, keyIt, nthreads);
        boophf_.reset(ph);
        std::cerr << "reordering keys and values to coincide with phf ... ";
        reorder_fn_();
        std::cerr << "done\n";
        built_ = true;
        return built_;
    }

    inline IteratorT find(const KeyT& k) {
        auto ind = boophf_->lookup(k);
        return (ind < data_.size()) ? (data_[ind].first == k ? data_.begin() + ind : data_.end()) : data_.end();
    }
    
    /**
     * NOTE: This function *assumes* that the key is in the hash.
     * If it isn't, you'll get back a random element!
     */
    inline ValueT& operator[](const KeyT& k) {
        auto ind = boophf_->lookup(k);
        return (ind < data_.size() ? data_[ind].second : data_[0].second);
    }
    
    inline IteratorT begin() { return data_.begin(); }
    inline IteratorT end() { return data_.end(); }
    inline IteratorT cend() const { return data_.cend(); }
    inline IteratorT cbegin() const { return data_.cbegin(); }

private:
    // Taken from http://stackoverflow.com/questions/12774207/fastest-way-to-check-if-a-file-exist-using-standard-c-c11-c
    bool FileExists_(const char *path) {
        struct stat fileStat;
        if ( stat(path, &fileStat) ) {
            return false;
        }
        if ( !S_ISREG(fileStat.st_mode) ) {
            return false;
        }
        return true;
    }

  
    void reorder_fn_()  {
	/* Adapted from code at: http://blog.merovius.de/2014/08/12/applying-permutation-in-constant.html */
        // Note, we can actually do this with out the bitvector by using the high-order bit 
        // of the start of the suffix array intervals (since they are signed integers and negative
        // positions are forbidden). 
      std::vector<bool> bits(data_.size(), false);
	for ( size_t i = 0; i < data_.size(); ++i ) {
	  if (!bits[i]) {
	    decltype(data_.front()) v = data_[i];
	    auto j = boophf_->lookup(data_[i].first);
	    while (i != j) {
            auto pj = boophf_->lookup(data_[j].first);
	      std::swap(data_[j], v);
	      bits[j] = 1;
	      j = pj; 
	    }
	    data_[i] = v;
	  }
	}

    }

    bool built_;
    std::vector<std::pair<KeyT, ValueT>> data_;
    std::unique_ptr<BooPHFT> boophf_{nullptr};
};
#endif // __BOO_MAP__ 
