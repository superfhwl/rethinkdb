#ifndef BTREE_ITERATION_HPP_
#define BTREE_ITERATION_HPP_

#include <list>

#include "errors.hpp"
#include <boost/shared_array.hpp>
#include <boost/shared_ptr.hpp>

#include "containers/iterators.hpp"
#include "btree/internal_node.hpp"
#include "btree/leaf_node.hpp"
#include "btree/operations.hpp"
#include "btree/slice.hpp"

template <class Value>
struct key_value_pair_t {
    store_key_t key;
    boost::shared_array<char> value;

    key_value_pair_t(value_sizer_t<void> *sizer, const store_key_t& _key, const void *_value) : key(_key) {
        int size = sizer->size(_value);
        value.reset(new char[size]);
        memcpy(value.get(), _value, size);
    }
};

/* leaf_iterator_t returns the keys of a btree leaf node one by one.
 * When it's done, it releases (deletes) the buf_lock object.
 *
 * TODO: should the buf_lock be released by the caller instead?
 */
template <class Value>
struct leaf_iterator_t : public one_way_iterator_t<key_value_pair_t<Value> > {
    leaf_iterator_t(const leaf_node_t *leaf, leaf::live_iter_t iter, buf_lock_t *lock, const boost::shared_ptr< value_sizer_t<void> >& sizer, transaction_t *transaction, btree_stats_t *stats);

    boost::optional<key_value_pair_t<Value> > next();
    void prefetch();
    virtual ~leaf_iterator_t();
private:
    void done();

    const leaf_node_t *leaf;
    leaf::live_iter_t iter;
    buf_lock_t *lock;
    boost::shared_ptr< value_sizer_t<void> > sizer;
    transaction_t *transaction;
    btree_stats_t *stats;
};

enum btree_bound_mode_t {
    btree_bound_open,   // Don't include boundary key
    btree_bound_closed,   // Include boundary key
    btree_bound_none   // Ignore boundary key and go all the way to the left/right side of the tree
};

/* slice_leaves_iterator_t finds the first leaf that contains the given key (or
 * the next key, if left_open is true). It returns that leaf iterator (which
 * also contains the lock), however it doesn't release the leaf lock itself
 * (it's done by the leaf iterator).
 *
 * slice_leaves_iterator_t maintains internal state by locking some internal
 * nodes and unlocking them as iteration progresses. Currently this locking is
 * done in DFS manner, as described in the btree/rget.cc file comment.
 */
template <class Value>
class slice_leaves_iterator_t : public one_way_iterator_t<leaf_iterator_t<Value>*> {
    struct internal_node_state {
        internal_node_state(const internal_node_t *_node, int _index, buf_lock_t *_lock)
            : node(_node), index(_index), lock(_lock) { }

        const internal_node_t *node;
        int index;
        buf_lock_t *lock;
    };
public:
    slice_leaves_iterator_t(const boost::shared_ptr< value_sizer_t<void> >& sizer, transaction_t *transaction, boost::scoped_ptr<superblock_t> &superblock, int slice_home_thread, const btree_key_t *left, btree_stats_t *_stats);

    boost::optional<leaf_iterator_t<Value>*> next();
    void prefetch();
    virtual ~slice_leaves_iterator_t();
private:
    void done();

    boost::optional<leaf_iterator_t<Value>*> get_first_leaf();
    boost::optional<leaf_iterator_t<Value>*> get_next_leaf();
    boost::optional<leaf_iterator_t<Value>*> get_leftmost_leaf(block_id_t node_id);
    block_id_t get_child_id(const internal_node_t *i_node, int index) const;

    boost::shared_ptr< value_sizer_t<void> > sizer;
    transaction_t *transaction;
    boost::scoped_ptr<superblock_t> superblock;
    int slice_home_thread;
    const btree_key_t *left;

    std::list<internal_node_state> traversal_state;
    bool started;
    bool nevermore;

    btree_stats_t *stats;
};

/* slice_keys_iterator_t combines slice_leaves_iterator_t and leaf_iterator_t to allow you
 * to iterate through the keys of a particular slice in order.
 *
 * Use merge_ordered_data_iterator_t class to funnel multiple slice_keys_iterator_t instances,
 * e.g. to get a range query for all the slices.
 */
template <class Value>
class slice_keys_iterator_t : public one_way_iterator_t<key_value_pair_t<Value> > {
public:
    slice_keys_iterator_t(const boost::shared_ptr< value_sizer_t<void> >& sizer, transaction_t *transaction, boost::scoped_ptr<superblock_t> &superblock, int slice_home_thread, const key_range_t &range, btree_stats_t *stats);
    virtual ~slice_keys_iterator_t();

    boost::optional<key_value_pair_t<Value> > next();
    void prefetch();
private:
    boost::optional<key_value_pair_t<Value> > get_first_value();
    boost::optional<key_value_pair_t<Value> > get_next_value();

    boost::optional<key_value_pair_t<Value> > validate_return_value(key_value_pair_t<Value> &pair) const;

    void done();

    boost::shared_ptr< value_sizer_t<void> > sizer;
    transaction_t *transaction;
    boost::scoped_ptr<superblock_t> superblock;
    int slice_home_thread;
    key_range_t range;
    btree_key_buffer_t left_buffer;

    bool no_more_data;
    leaf_iterator_t<Value> *active_leaf;
    slice_leaves_iterator_t<Value> *leaves_iterator;

    btree_stats_t *stats;
};

#include "btree/iteration.tcc"


#endif // BTREE_ITERATION_HPP_
