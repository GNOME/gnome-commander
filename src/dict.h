#ifndef __DICT_H
#define __DICT_H


#include <map>
#include <string>


class BASE_DICT
{
  protected:

    static const std::string empty_string;
};


template <typename T>
class DICT: public BASE_DICT
{
    typedef std::map<T,std::string> IDX_COLL;
    typedef std::map<std::string,T> NAME_COLL;

    IDX_COLL idx;
    NAME_COLL name;

    const T NONE;

  public:

    DICT(const T none): NONE(none)               {}

    void add(const T n, const std::string &s);
    void add(const T n, const char *s)           {  add(n,std::string(s));  }

    const std::string &operator [] (const T n) const;
    const T operator [] (const std::string &s) const;
    const T operator [] (const char *s) const          {  return operator [] (std::string(s));  }
};


template <typename T>
inline void DICT<T>::add(const T n, const std::string &s)
{
  idx.insert(make_pair(n,s));
  name.insert(make_pair(s,n));
}


template <typename T>
inline const std::string &DICT<T>::operator [] (const T n) const
{
  typename IDX_COLL::const_iterator i = idx.find(n);

  return i!=idx.end() ? i->second : empty_string;
}


template <typename T>
inline const T DICT<T>::operator [] (const std::string &s) const
{
  typename NAME_COLL::const_iterator i = name.find(s);

  return i!=name.end() ? i->second : NONE;
}


template <>
class DICT<std::string>: public BASE_DICT
{
    typedef std::map<std::string,std::string> IDX_COLL;

    IDX_COLL idx;

    const std::string NONE;

  public:

    DICT(const std::string none): NONE(none)               {}

    void add(const std::string &n, const std::string &s)   {  idx.insert(make_pair(n,s));           }
    void add(const char *n, const char *s)                 {  add(std::string(n),std::string(s));   }

    const std::string operator [] (const std::string &s) const
    {
      IDX_COLL::const_iterator i = idx.find(s);

      return i!=idx.end() ? i->second : NONE;
    }

    const std::string operator [] (const char *s) const    {  return operator [] (std::string(s));  }
};


#endif
