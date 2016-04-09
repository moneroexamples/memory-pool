//
// Created by mwo on 9/04/16.
//
// based on: http://stackoverflow.com/a/14523787

#ifndef MPOOL_MEMBER_CHECKER_H
#define MPOOL_MEMBER_CHECKER_H


#define DEFINE_MEMBER_CHECKER(member) \
    template<typename T, typename V = bool> \
    struct has_ ## member : false_type { }; \
    template<typename T> \
    struct has_ ## member<T, \
        typename enable_if< \
            !is_same<decltype(declval<T>().member), void>::value, \
            bool \
            >::type \
        > : true_type { };

#define HAS_MEMBER(C, member) \
    has_ ## member<C>::value

#endif //MPOOL_MEMBER_CHECKER_H
