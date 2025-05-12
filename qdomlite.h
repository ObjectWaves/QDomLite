//Quick and Dirty Document Object Model by Thomas Allin. Fast and cheap one-file XML. LGPL v3 license. thomallin@gmail.com

#ifndef QDOMLITE_H
#define QDOMLITE_H

#include <QString>
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#include <QStringRef>
#else
#include <QStringEncoder>
#endif
#include <QVariant>
#include <QList>
#include <QStringList>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <string>
#include <sstream>
#include <QRegularExpression>

#define XMLmaxtaglen 1000
#define XMLendtaglen 100

static const QRegularExpression rxOther(QStringLiteral("\\s*<!"));
static const QRegularExpression rxTag(QStringLiteral("\\s*<([^<>/\\s]+)\\s*([^>]*)\\s*>\\s*"));
static const QRegularExpression rxRemark(QStringLiteral("\\s*<!--(.+)-->\\s*"),QRegularExpression::DotMatchesEverythingOption | QRegularExpression::InvertedGreedinessOption);
static const QRegularExpression rxAttribute(QStringLiteral("\\s*([^=]+)\\s*=\\s*[\"\']([^\"\']*)[\"\']\\s*"));
static const QRegularExpression rxdocType(QStringLiteral("\\s*<!doctype(.+)[\[>]\\s*"),QRegularExpression::CaseInsensitiveOption | QRegularExpression::InvertedGreedinessOption);
static const QRegularExpression rxXMLAttributes(QStringLiteral("\\s*<\\?xml[^\\s]*(.+)\\?>\\s*"),QRegularExpression::CaseInsensitiveOption | QRegularExpression::InvertedGreedinessOption);
static const QRegularExpression rxCDATA(QStringLiteral("\\s*<![\[]CDATA[\[](.+)[\\]][\\]]>"),QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption | QRegularExpression::InvertedGreedinessOption);
static const QRegularExpression rxEntityTags(QStringLiteral("\\s*[\[](.+)[\\]]\\s*>\\s*"), QRegularExpression::InvertedGreedinessOption);
static const QRegularExpression rxEntity(QStringLiteral("\\s*<!ENTITY\\s+([^\\s]+).*[\"\'](.+)[\"\']>\\s*"),QRegularExpression::CaseInsensitiveOption | QRegularExpression::InvertedGreedinessOption);

static const QString emptyString;

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#define XMLStringClass QStringView
#else
#define XMLStringClass QDomLiteXMLString

class QDomLiteXMLString : public QString
{
public:
    inline QDomLiteXMLString() : QString() {}
    inline QDomLiteXMLString(const QByteArray& b) : QString(b) {}
    inline QDomLiteXMLString(const QString& s) : QString(s) {}
    inline const QString leftSubString(int n) const
    {
        const int sz=size();
        if (n >= sz || n < 0) n = sz;
        if (n == sz) return *this;
        return QString::fromRawData(unicode(),n);
    }
    inline const QString rightSubString(int n) const
    {
        const int sz=size();
        if (n >= sz || n < 0) n = sz;
        if (n == sz) return *this;
        return QString::fromRawData(unicode()+(sz-n),n);
    }
    inline const QString chopped(const int n) const
    {
        if (n < 0) return *this;
        const int sz=size()-n;
        return (sz <= 0) ? emptyString :  QString::fromRawData(unicode(),sz);
    }
    inline const QString sliced(const int position) const
    {
        const int sz=size();
        if ((sz == 0) || position >= sz) return emptyString;
        if (position <= 0) return *this;
        return QString::fromRawData(unicode()+position,sz-position);
    }
    inline const QString sliced(int position, int n) const
    {
        const int sz=size();
        if ((sz == 0) || position >= sz) return emptyString;
        if (n < 0) n = sz - position;
        if (position < 0) {
            n += position;
            position = 0;
        }
        if (n + position > sz) n = sz - position;
        if (position == 0 && n == sz) return *this;
        return QString::fromRawData(unicode()+position,n);
    }
    inline const QString trimmedSubString() const
    {
        if (isEmpty()) return *this;
        const QChar* s = unicode();
        if (!s->isSpace() && !s[size()-1].isSpace()) return *this;
        int start = 0;
        int end = size() - 1;
        while (start<=end && s[start].isSpace())  // skip white space from start
            start++;
        if (start <= end) {                          // only white space
            while (end && s[end].isSpace())           // skip white space from end
                end--;
        }
        const int l = end - start + 1;
        return (l <= 0) ? emptyString : QString::fromRawData(s + start, l);
    }
    inline int leftStringToInt(int n, bool* ok=nullptr, int base=10) const
    {
        return leftSubString(n).toInt(ok,base);
    }
    inline int rightStringToInt(int n, bool* ok=nullptr, int base=10) const
    {
        return rightSubString(n).toInt(ok,base);
    }
};
#endif

#pragma pack(push,1)

class QDomLiteElement;
class QDomLiteAttribute;

namespace QDomLite
{
void inline swapElements(QDomLiteElement **x, QDomLiteElement **y)
{
    QDomLiteElement *t = *x;
    *x = *y;
    *y = t;
}
}

class CStringMatcher
{
public:
    inline CStringMatcher(const QChar& c) : needle(c), needleChar(c), ptr(needle.unicode()), size(1) {}
    inline CStringMatcher(const QString& s) : needle(s), needleChar(s.at(0)), ptr(needle.unicode()), size(s.size()) {}
    inline bool matches(const QChar& c) const { return (needleChar == c); }
    inline bool matches(const QString& s) const
    {
        return (size == 1) ? (s.at(0) == needleChar) : (memcmp(s.unicode(),ptr,uint(size) * 2) == 0);
    }
    inline bool matches(const QString& s, const int pos) const
    {
        return (size == 1) ? (s.at(pos) == needleChar) : (memcmp(s.unicode()+pos,ptr,uint(size) * 2) == 0);
    }
    inline bool matches(const QString& s, const int pos, const int n) const
    {
        if (n != size) return false;
        return (n == 1) ? (s.at(pos) == needleChar) : (memcmp(s.unicode() + pos,ptr,uint(n) * 2) == 0);
    }
    inline int indexIn(const QString& s) const
    {
        return (size == 1) ? charIndex(s.unicode(),s.size()) : s.indexOf(needle);
    }
    inline int indexIn(const QString &s, const int pos, const int n) const
    {
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        return (size == 1) ? charIndex(s.unicode() + pos, n) : s.midRef(pos,n).indexOf(needle);
#else
        return (size == 1) ? charIndex(s.unicode() + pos, n) : s.mid(pos,n).indexOf(needle);
#endif
    }
    inline int charIndex(const QChar* s, const int n) const
    {
        for (int i = 0; i < n; i++) if (s[i] == needleChar) return i;
        return -1;
    }
    inline const CStringMatcher& operator=(const QChar& c) { return *new CStringMatcher(c); }
    inline const CStringMatcher& operator=(const QString& s) { return *new CStringMatcher(s); }
    const QString needle;
    const QChar needleChar;
    const QChar* ptr;
    const int size;
};

class CStringListMatcher
{
public:
    inline CStringListMatcher(const QStringList& l, const QStringList& r = QStringList()) : replaceList(r),listSize(l.size())
    {
        for (int i = 0; i < listSize; i++) matchList.append(new CStringMatcher(l.at(i)));
    }
    ~CStringListMatcher()
    {
        qDeleteAll(matchList);
    }
    inline int matchIndex(const QChar& c) const
    {
        for (int i = 0; i < listSize; i++) if (matchList.at(i)->matches(c)) return i;
        return -1;
    }
    inline int matchIndex(const QString& s) const
    {
        for (int i = 0; i < listSize; i++) if (matchList.at(i)->matches(s)) return i;
        return -1;
    }
    inline int matchIndex(const QString& s, const int pos) const
    {
        for (int i = 0; i < listSize; i++) if (matchList.at(i)->matches(s,pos)) return i;
        return -1;
    }
    inline int matchIndex(const QString& s, const int pos, const int n) const
    {
        for (int i = 0; i < listSize; i++) if (matchList.at(i)->matches(s,pos,n)) return i;
        return -1;
    }
    QList<CStringMatcher*> matchList;
    const QStringList replaceList;
    const int listSize;
};

static const CStringMatcher ampCharMatcher('&');

static const CStringListMatcher entityMatcher({"&gt;","&lt;","&amp;","&quot;","&apos;"},{">","<","&","\"","'"});
static const CStringListMatcher entityCharMatcher({">","<","&","\"","'"},{"&gt;","&lt;","&amp;","&quot;","&apos;"});

static const CStringListMatcher trueMatcher({"yes","true","1","-1",QVariant(true).toString().toLower()});

class QDomLiteValue : public QString
{
public:
    inline QDomLiteValue() : QString() {}
    inline QDomLiteValue(const char* str) : QString(QLatin1String(str)) {}
    inline QDomLiteValue(const QString& str) : QString(str) {}
    inline QDomLiteValue(const QVariant& val) : QString(val.toString()) {}
    inline QDomLiteValue(const bool val) : QString(QVariant(val).toString()) {}
    inline QDomLiteValue(const short val) : QString(QString::number(val)) {}
    inline QDomLiteValue(const ushort val) : QString(QString::number(val)) {}
    inline QDomLiteValue(const int val) : QString(QString::number(val)) {}
    inline QDomLiteValue(const uint val) : QString(QString::number(val)) {}
    inline QDomLiteValue(const long val) : QString(QString::number(val)) {}
    inline QDomLiteValue(const ulong val) : QString(QString::number(val)) {}
    inline QDomLiteValue(const long long val) : QString(QString::number(val)) {}
    inline QDomLiteValue(const unsigned long long val) : QString(QString::number(val)) {}
    inline QDomLiteValue(const float val) : QString(QString::number(double(val))) {}
    inline QDomLiteValue(const double val) : QString(QString::number(val)) {}
    inline QDomLiteValue(const long double val) : QString(number(val)) {}
    static QString number(const long double val)
    {
        std::stringstream ss;
        ss << val;

        return QString::fromStdString(ss.str());
    }
    long double toLDouble(bool *ok=nullptr) const
    {
        long double ld=0;
        *ok=true;
        try
        {
            ld=stold(toStdString());
        }
        catch (...)
        {
            *ok=false;
        }
        return ld;
    }
    inline double numeric() const
    {
        if (isEmpty()) return 0;
        bool isNumber;
        const double retval=toDouble(&isNumber);
        return (isNumber) ? retval : 0;
    }
    inline long double numericLDouble() const
    {
        if (isEmpty()) return 0;
        bool isNumber;
        const long double retval=toLDouble(&isNumber);
        return (isNumber) ? retval : 0;
    }
    inline long numericLong() const
    {
        if (isEmpty()) return 0;
        bool isNumber;
        const long retval=toLong(&isNumber);
        return (isNumber) ? retval : 0;
    }
    inline long long numericLongLong() const
    {
        if (isEmpty()) return 0;
        bool isNumber;
        const long long retval=toLongLong(&isNumber);
        return (isNumber) ? retval : 0;
    }
    inline ulong numericULong() const
    {
        if (isEmpty()) return 0;
        bool isNumber;
        const ulong retval=toULong(&isNumber);
        return (isNumber) ? retval : 0;
    }
    inline unsigned long long numericULongLong() const
    {
        if (isEmpty()) return 0;
        bool isNumber;
        const unsigned long long retval=toULongLong(&isNumber);
        return (isNumber) ? retval : 0;
    }
    inline int numericInt() const
    {
        if (isEmpty()) return 0;
        bool isNumber;
        const int retval=toInt(&isNumber);
        return (isNumber) ? retval : 0;
    }
    inline uint numericUInt() const
    {
        if (isEmpty()) return 0;
        bool isNumber;
        const uint retval=toUInt(&isNumber);
        return (isNumber) ? retval : 0;
    }
    inline bool numericBool() const
    {
        if (isEmpty()) return false;
        if (trueMatcher.matchIndex(toLower()) > -1) return boolTrueValue;
        bool isNumber;
        const double retval=toDouble(&isNumber);
        return (isNumber) ? bool(retval) : 0;
    }
    inline const QString string() const { return (*this); }
    //const inline QString encodedString() const
    //{
    //    return toEncodedString();
    //}
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    inline void fromEncodedString( const QStringView& str ) {
        fromEncodedString(str.toString());
    }
#endif
    const inline QString encodedString() const {
        QString rich;
        rich.reserve(this->length() * 1.2);
        for (int ptr = 0; ptr < this->length(); ptr++)
        {
            const int m = entityCharMatcher.matchIndex(this->at(ptr));
            if (m > -1)
            {
                rich += entityCharMatcher.replaceList.at(m);
            }
            else
            {
                rich += this->at(ptr);
            }
        }
        rich.squeeze();
        return rich;
    }
    inline void fromEncodedString( const QString& str )
    {
        *this=str;
        if (ampCharMatcher.indexIn(str) == -1)
        {
            *this=str;
            return;
        }
        const int len = str.size();
        clear();
        reserve(len);
        int ptr = 0;
        while (ptr < len) {
            if (ampCharMatcher.matches(str,ptr))
            {
                const int m=entityMatcher.matchIndex(str,ptr);
                if (m > -1)
                {
                    ptr += entityMatcher.matchList.at(m)->size;
                    *this+=entityMatcher.replaceList.at(m);
                }
                else {
                    *this+=str.at(ptr++);
                }
            }
            else {
                *this+=str.at(ptr++);
            }
        }
        squeeze();
    }
    inline void fromEncodedString( const QString& str, int position, int n )
    {
        const int sz = str.size();
        if ((sz == 0) || position >= sz)
        {
            clear();
            return;
        }
        if (n < 0) n = sz - position;
        if (position < 0) {
            n += position;
            position = 0;
        }
        if (n + position > sz) n = sz - position;
        if (ampCharMatcher.indexIn(str,position,n) == -1)
        {
            *this= ((position == 0) && (n == sz)) ? str : str.mid(position,n);
            return;
        }
        clear();
        reserve(n);
        const int endPosition = position+n;
        while (position < endPosition) {
            if (ampCharMatcher.matches(str,position))
            {
                const int m=entityMatcher.matchIndex(str,position);
                if (m > -1)
                {
                    position += entityMatcher.matchList.at(m)->size;
                    *this+=entityMatcher.replaceList.at(m);
                }
                else {
                    *this+=str.at(position++);
                }
            }
            else {
                *this+=str.at(position++);
            }
        }
        squeeze();
    }
    inline const QString operator+(const QDomLiteValue& val) const { return string()+val.string(); }
    inline const QString operator+(const char* str) const { return string()+QLatin1String(str); }
private:
    static const int boolTrueValue=1;
};

namespace QDomLite
{
inline const QDomLiteValue valueFromString(const QString& s) {
    QDomLiteValue v;
    v.fromEncodedString(s);
    return v;
}
}

typedef QList<QDomLiteElement*> QDomLiteElementList;
typedef QList<QDomLiteAttribute*> QDomLiteAttributeList;
typedef QMap<QString,QDomLiteValue> QDomLiteAttributeMap;
typedef QStringList QDomLiteNameList;
typedef QList<QDomLiteValue> QDomLiteValueList;
typedef QStringList QDomLiteTagList;
typedef QMap<QString,QString> QDomLiteEntityMap;

class QDomLiteAttribute
{
public:
    inline QDomLiteAttribute() {}
    inline QDomLiteAttribute(const QString& name, const QDomLiteValue& value) {
        this->name=name;
        this->value=value;
    }
    inline QDomLiteAttribute(const QString& XML, int& position) { position=fromString(XML, position); }
    inline QDomLiteAttribute(const QDomLiteAttribute* other) { copy(other); }
    QString name;
    QDomLiteValue value;
    inline void copy(const QDomLiteAttribute* other)
    {
        name=other->name;
        value=other->value;
    }
    inline const QString toString() const { return name+QStringLiteral("=\"")+value.encodedString()+'"'; }
    inline QDomLiteAttribute* clone() const { return new QDomLiteAttribute(this); }
    inline int fromString(const QString& XML, int start=0)
    {
        const auto AttrMatch = rxAttribute.matchView(XML,start);
        if (AttrMatch.capturedStart()==start)
        {
            start+=AttrMatch.capturedLength();
            name = AttrMatch.captured(1);
            value.fromEncodedString(AttrMatch.captured(2));
        }
        return start;
    }
    inline bool matches(const QString& name) const { return (this->name == name); }
};

namespace QDomLite
{
static const QDomLiteAttribute emptyAttribute;
inline const QDomLiteAttribute attributeFromString(const QString& s) {
    QDomLiteAttribute a;
    a.fromString(s);
    return a;
}
}

class QDomLiteAttributes
{
public:
    inline const QDomLiteValue attribute(const QString& name) const { return item(name)->value; }
    inline const QDomLiteValue attribute(const int index) const { return item(index)->value; }
    inline const QDomLiteValue attribute(const QString& name, const QString& defaultValue) const {
        return (!attributeExists(name)) ? defaultValue : item(name)->value;
    }
    inline const QDomLiteValue attribute(const int index, const QString& defaultValue) const {
        return (!attributeExists(index)) ? defaultValue : item(index)->value;
    }
    inline const QString attributeName(const int index) const { return item(index)->name; }
    inline const QString attributesString() const {
        QString RetVal;
        RetVal.reserve(attributes.size()*80);
        for (const auto a : attributes) RetVal+=QChar::Space+a->toString();
        RetVal.squeeze();
        return RetVal;
    }
    inline const QDomLiteAttributeMap attributesMap() const
    {
        QDomLiteAttributeMap retval;
        for (const auto a : attributes) retval[a->name]=a->value;
        return retval;
    }
    inline double attributeValue(const QString& name) const { return item(name)->value.numeric(); }
    inline double attributeValue(const int index) const { return item(index)->value.numeric(); }
    inline double attributeValue(const QString& name, const double defaultValue) const {
        return (!attributeExists(name)) ? defaultValue : item(name)->value.numeric();
    }
    inline double attributeValue(const int index, const double defaultValue) const {
        return (!attributeExists(index)) ? defaultValue : item(index)->value.numeric();
    }
    inline long double attributeValueLDouble(const QString& name) const { return item(name)->value.numericLDouble(); }
    inline long double attributeValueLDouble(const int index) const { return item(index)->value.numericLDouble(); }
    inline long double attributeValueLDouble(const QString& name, const long double defaultValue) const {
        return (!attributeExists(name)) ? defaultValue : item(name)->value.numericLDouble();
    }
    inline long double attributeValueLDouble(const int index, const long double defaultValue) const {
        return (!attributeExists(index)) ? defaultValue : item(index)->value.numericLDouble();
    }

    inline long attributeValueLong(const QString& name) const { return item(name)->value.numericLong(); }
    inline long attributeValueLong(const int index) const { return item(index)->value.numericLong(); }
    inline long attributeValueLong(const QString& name, const long defaultValue) const {
        return (!attributeExists(name)) ? defaultValue : item(name)->value.numericLong();
    }
    inline long attributeValueLong(const int index, const long defaultValue) const {
        return (!attributeExists(index)) ? defaultValue : item(index)->value.numericLong();
    }
    inline ulong attributeValueULong(const QString& name) const { return item(name)->value.numericULong(); }
    inline ulong attributeValueULong(const int index) const { return item(index)->value.numericULong(); }
    inline ulong attributeValueULong(const QString& name, const ulong defaultValue) const {
        return (!attributeExists(name)) ? defaultValue : item(name)->value.numericULong();
    }
    inline ulong attributeValueULong(const int index, const ulong defaultValue) const {
        return (!attributeExists(index)) ? defaultValue : item(index)->value.numericULong();
    }
    inline long long attributeValueLongLong(const QString& name) const { return item(name)->value.numericLongLong(); }
    inline long long attributeValueLongLong(const int index) const { return item(index)->value.numericLongLong(); }
    inline long long attributeValueLongLong(const QString& name, const long long defaultValue) const {
        return (!attributeExists(name)) ? defaultValue : item(name)->value.numericLongLong();
    }
    inline long long attributeValueLongLong(const int index, const long long defaultValue) const {
        return (!attributeExists(index)) ? defaultValue : item(index)->value.numericLongLong();
    }
    inline unsigned long long attributeValueULongLong(const QString& name) const { return item(name)->value.numericULongLong(); }
    inline unsigned long long attributeValueULongLong(const int index) const { return item(index)->value.numericULongLong(); }
    inline unsigned long long attributeValueULongLong(const QString& name, const unsigned long long defaultValue) const {
        return (!attributeExists(name)) ? defaultValue : item(name)->value.numericULongLong();
    }
    inline unsigned long long attributeValueULongLong(const int index, const unsigned long long defaultValue) const {
        return (!attributeExists(index)) ? defaultValue : item(index)->value.numericULongLong();
    }
    inline int attributeValueInt(const QString& name) const { return item(name)->value.numericInt(); }
    inline int attributeValueInt(const int index) const { return item(index)->value.numericInt(); }
    inline int attributeValueInt(const QString& name, const int defaultValue) const {
        return (!attributeExists(name)) ? defaultValue : item(name)->value.numericInt();
    }
    inline int attributeValueInt(const int index, const int defaultValue) const {
        return (!attributeExists(index)) ? defaultValue : item(index)->value.numericInt();
    }
    inline uint attributeValueUInt(const QString& name) const { return item(name)->value.numericUInt(); }
    inline uint attributeValueUInt(const int index) const { return item(index)->value.numericUInt(); }
    inline uint attributeValueUInt(const QString& name, const uint defaultValue) const {
        return (!attributeExists(name)) ? defaultValue : item(name)->value.numericUInt();
    }
    inline uint attributeValueUInt(const int index, const uint defaultValue) const {
        return (!attributeExists(index)) ? defaultValue : item(index)->value.numericUInt();
    }
    inline bool attributeValueBool(const QString& name) const { return item(name)->value.numericBool(); }
    inline bool attributeValueBool(const int index) const { return item(index)->value.numericBool(); }
    inline bool attributeValueBool(const QString& name, const bool defaultValue) const {
        return (!attributeExists(name)) ? defaultValue : item(name)->value.numericBool();
    }
    inline bool attributeValueBool(const int index, const bool defaultValue) const {
        return (!attributeExists(index)) ? defaultValue : item(index)->value.numericBool();
    }
    inline void appendAttribute(const QString &name, const QDomLiteValue& value) { attributes.append(new QDomLiteAttribute(name,value)); }
    inline void setAttribute(const QString& name, const QDomLiteValue& value)
    {
        if (value.isEmpty())
        {
            removeAttribute(name);
            return;
        }
        if (attributes.isEmpty())
        {
            appendAttribute(name,value);
            return;
        }
        for (auto a : std::as_const(attributes))
        {
            if (a->matches(name))
            {
                a->value=value;
                return;
            }
        }
        appendAttribute(name,value);
    }
    inline void setAttribute(const int index, const QDomLiteValue& value)
    {
        if (!attributeExists(index)) return;
        if (value.isEmpty())
        {
            removeAttribute(index);
            return;
        }
        attributes.at(index)->value=value;
    }
    inline void setAttribute(const QString& name, const QDomLiteValue& value, const QDomLiteValue& defaultValue)
    {
        if (value==defaultValue)
        {
            removeAttribute(name);
            return;
        }
        setAttribute(name,value);
    }
    inline void setAttributes(const QDomLiteNameList& names, const QDomLiteValueList& values)
    {
        for (int i=0;i<names.size();i++) setAttribute(names.at(i),values.at(i));
    }
    inline void setAttributes(const QDomLiteAttributeMap& map)
    {
        for (auto it = map.constKeyValueBegin(); it != map.constKeyValueEnd(); it++) setAttribute(it->first,it->second);
        //for (const QString& s : map.keys()) setAttribute(s,map[s]);
    }
    inline void setAttributesString(const QString& attributesString)
    {
        clearAttributes();
        appendAttributesString(attributesString);
    }
    inline void appendAttributesString(const QString& attributesString)
    {
        int start = 0;
        const int len = attributesString.length() - 1; // skip possible "/"
        while (start < len)
        {
            const int i = start;
            auto a = new QDomLiteAttribute(attributesString,start);
            if (i == start)
            {
                delete a;
                break;
            }
            attributes.append(a);
        }
    }
    inline void setAttributesMap(const QDomLiteAttributeMap& map)
    {
        clearAttributes();
        appendAttributesMap(map);
    }
    inline void appendAttributesMap(const QDomLiteAttributeMap& map)
    {
        for (auto it = map.constKeyValueBegin(); it != map.constKeyValueEnd(); it++) appendAttribute(it->first,it->second);
        //for(const QString& s : map.keys()) appendAttribute(s,map[s]);
    }
    inline void setAttributesLists(const QDomLiteNameList& names, const QDomLiteValueList& values)
    {
        clearAttributes();
        appendAttributesLists(names,values);
    }
    inline void appendAttributesLists(const QDomLiteNameList& names, const QDomLiteValueList& values)
    {
        for (int i=0;i<names.size();i++) appendAttribute(names.at(i),values.at(i));
    }
    inline const QDomLiteNameList attributesNameList()
    {
        QDomLiteNameList l;
        for (const auto a : std::as_const(attributes)) l.append(a->name);
        return l;
    }
    inline const QDomLiteValueList attributesValueList()
    {
        QDomLiteValueList l;
        for (const auto a : std::as_const(attributes)) l.append(a->value);
        return l;
    }
    inline void appendAttributes(const QDomLiteAttributeList& attr) { attributes.append(attr); }
    inline void removeAttribute(const QString& name) { removeAttribute(indexOfAttribute(name)); }
    inline void removeAttribute(const int index)
    {
        if (!attributeExists(index)) return;
        delete attributes.at(index);
        attributes.erase(attributes.constBegin() + index);
    }
    inline void clearAttributes()
    {
        qDeleteAll(attributes);
        attributes.clear();
    }
    inline int attributeCount() const { return attributes.size(); }
    inline int indexOfAttribute(const QString& name) const {
        for (int i = 0; i < attributes.size(); i++) {
            if (attributes.at(i)->matches(name)) return i;
        }
        return -1;
    }
    inline bool attributeExists(const QString& name) const { return (indexOfAttribute(name) > -1); }
    inline bool attributeExists(const int index) const { return ((index < attributes.size()) && (index >= 0)); }
    inline void renameAttribute(const QString& name, const QString& newName)
    {
        if (!attributeExists(newName))
        {
            if (attributeExists(name)) item(name)->name=newName;
        }
    }
    QDomLiteAttributeList attributes;
protected:
    inline QDomLiteAttribute* item(const QString& name) const {
        for (auto a : attributes) if (a->matches(name)) return a;
        return const_cast<QDomLiteAttribute*>(&(QDomLite::emptyAttribute));
    }
    inline QDomLiteAttribute* item(const int index) const {
        return (!attributeExists(index)) ? const_cast<QDomLiteAttribute*>(&(QDomLite::emptyAttribute)) : attributes.at(index);
    }
};

class QDomLiteElement : public QDomLiteAttributes
{
public:
    enum QDomLiteElementType
    {
        UndefinedElement=0,
        ComplexElement=1,
        TextElement=2,
        CDATAElement=4
    };
    inline QDomLiteElement(const QString& Tag) { tag=Tag; }
    inline QDomLiteElement(const XMLStringClass& XML, int& position) { position=fromString(XML, position); }
    inline QDomLiteElement(const QString& Tag, const QString& AttrName, const QDomLiteValue& AttrValue) {
        tag=Tag;
        appendAttribute(AttrName,AttrValue);
    }
    inline QDomLiteElement(const QString &Tag, const QDomLiteNameList& names, const QDomLiteValueList& values)
    {
        tag=Tag;
        appendAttributesLists(names,values);
    }
    inline QDomLiteElement(const QString& Tag, const QDomLiteAttributeMap& map)
    {
        tag=Tag;
        appendAttributesMap(map);
    }
    inline QDomLiteElement() {}
    inline QDomLiteElement(const QDomLiteElement* other) { copy(other); }
    inline QDomLiteElement(const QDomLiteElement& other) { copy(&other); }
    inline ~QDomLiteElement() { clear(); }
    inline bool isText() const { return !text.isEmpty(); }
    inline bool isCDATA() const { return !CDATA.isEmpty(); }
    inline bool isComplex() const { return (text.size()+CDATA.size()==0); }
    inline QDomLiteElementType elementType() const {
        if (isText()) return QDomLiteElement::TextElement;
        if (isCDATA()) return QDomLiteElement::CDATAElement;
        if (isComplex()) return QDomLiteElement::ComplexElement;
        return QDomLiteElement::UndefinedElement;
    }
    QString tag;
    QDomLiteValue text;
    QString CDATA;
    QDomLiteValueList comments;
    QDomLiteElementList childElements;
    inline QDomLiteTagList childTags()
    {
        QDomLiteTagList l;
        for (const auto e : std::as_const(childElements)) l.append(e->tag);
        return l;
    }
    inline QDomLiteElementList allChildren() const
    {
        QDomLiteElementList RetVal;
        for (auto e : childElements)
        {
            (e->childCount()==0) ? RetVal.append(e) : RetVal.append(e->allChildren());
        }
        return RetVal;
    }
    inline QDomLiteElementList elementsByTag(const QString& name) const
    {
        QDomLiteElementList RetVal;
        for (auto e : childElements) if (e->matches(name)) RetVal.append(e);
        return RetVal;
    }
    inline QDomLiteElement* elementByTag(const QString& name) const
    {
        for (auto e : childElements) if (e->matches(name)) return e;
        return nullptr;
    }
    inline QDomLiteElementList elementsByTag(const QString& name, const bool deep) const
    {
        QDomLiteElementList RetVal;
        for (auto e : childElements)
        {
            if (e->matches(name)) RetVal.append(e);
            if (deep) RetVal.append(e->elementsByTag(name,deep));
        }
        return RetVal;
    }
    inline QDomLiteElement* elementByTag(const QString& name, const bool deep) const
    {
        QDomLiteElement* RetVal=nullptr;
        for (auto e : childElements)
        {
            if (e->matches(name)) return e;
            if (deep)
            {
                RetVal=e->elementByTag(name,deep);
                if (RetVal!=nullptr) return RetVal;
            }
        }
        return RetVal;
    }
    inline QDomLiteElement* elementByTagCreate(const QString& name)
    {
        for (auto e : std::as_const(childElements)) if (e->matches(name)) return e;
        return appendChild(name);
    }
    inline QDomLiteElement* elementByTagCreate(QDomLiteElement* element)
    {
        for (auto e : std::as_const(childElements)) if (e->matches(element->tag)) return e;
        return appendClone(element);
    }
    inline QDomLiteElement* elementByPath(QStringList list)
    {
        if (list.empty()) return nullptr;
        auto e = elementByTag(list.takeFirst());
        if (e) return (list.empty()) ? e : e->elementByPath(list);
        return nullptr;
    }
    inline QDomLiteElement* elementByPath(const QString& listString, const QChar& separator = '/')
    {
        return elementByPath(listString.split(separator));
    }
    inline QDomLiteElement* elementByPathCreate(QStringList list)
    {
        if (list.empty()) return nullptr;
        auto e = elementByTagCreate(list.takeFirst());
        return (list.empty()) ? e : e->elementByPathCreate(list);
    }
    inline QDomLiteElement* elementByPathCreate(const QString& listString, const QChar& separator = '/')
    {
        return elementByPathCreate(listString.split(separator));
    }
    inline double value() const { return text.numeric(); }
    inline const QDomLiteValue childText(const QString& childTag) const
    {
        for (int i=0;i<childElements.size();i++) if (childElements.at(i)->tag==childTag) return childElements.at(i)->text;
        return QDomLiteValue();
    }
    inline double childValue(const QString& childTag) const { return childText(childTag).numeric(); }
    inline QDomLiteElement* setChild(const QString& name, QDomLiteElement* element) { return replaceChild(elementByTagCreate(name),element); }
    inline QDomLiteElement* replaceChild(QDomLiteElement* destinationElement, QDomLiteElement* sourceElement)
    {
        const int index=childElements.indexOf(destinationElement);
        if (index>-1)
        {
            delete childElements.at(index);
            childElements[index]=sourceElement;
        }
        return sourceElement;
    }
    inline QDomLiteElement* replaceChild(QDomLiteElement* destinationElement, const QString& name) { return replaceChild(destinationElement, new QDomLiteElement(name)); }
    inline QDomLiteElement* replaceChild(const int index, QDomLiteElement* sourceElement)
    {
        if (index>-1)
        {
            delete childElements.at(index);
            childElements[index]=sourceElement;
        }
        return sourceElement;
    }
    inline QDomLiteElement* replaceChild(const int index, const QString& name) { return replaceChild(index, new QDomLiteElement(name)); }
    inline QDomLiteElement* exchangeChild(QDomLiteElement* destinationElement, QDomLiteElement* sourceElement)
    {
        const int index=childElements.indexOf(destinationElement);
        if (index>-1) childElements[index]=sourceElement;
        return destinationElement;
    }
    inline QDomLiteElement* exchangeChild(const int index, QDomLiteElement* sourceElement)
    {
        QDomLiteElement* destinationElement=nullptr;
        if (index>-1)
        {
            destinationElement=childElements.at(index);
            childElements[index]=sourceElement;
        }
        return destinationElement;
    }
    inline void removeChild(QDomLiteElement* element)
    {
        removeChild(childElements.indexOf(element));
    }
    inline void removeChild(const int index)
    {
        if (!elementExists(index)) return;
        delete childElements.at(index);
        childElements.erase(childElements.constBegin() + index);
    }
    inline void removeChild(const QString& name)
    {
        auto element=elementByTag(name);
        if (element != nullptr) removeChild(element);
    }
    inline void removeFirst() { removeChild(0); }
    inline void removeLast() { removeChild(childElements.size()-1); }
    inline QDomLiteElement* takeChild(QDomLiteElement* element)
    {
        return takeChild(childElements.indexOf(element));
    }
    inline QDomLiteElement* takeChild(const int index)
    {
        return (!elementExists(index)) ? nullptr : childElements.takeAt(index);
    }
    inline QDomLiteElement* takeChild(const QString& name)
    {
        auto element=elementByTag(name);
        return (element != nullptr) ? takeChild(element) : nullptr;
    }
    inline QDomLiteElement* takeFirst() { return takeChild(0); }
    inline QDomLiteElement* takeLast() { return takeChild(childElements.size()-1); }
    inline QDomLiteElement* appendChild(const QString& name)
    {
        return appendChild(new QDomLiteElement(name));
    }
    inline QDomLiteElement* appendChild(const QString& name, const QString& attrName, const QDomLiteValue& attrValue)
    {
        return appendChild(new QDomLiteElement(name,attrName,attrValue));
    }
    inline QDomLiteElement* appendChild(const QString& name, const QDomLiteNameList& names, const QDomLiteValueList& values)
    {
        return appendChild(new QDomLiteElement(name,names,values));
    }
    inline QDomLiteElement* appendChild(const QString& name, const QDomLiteAttributeMap& map)
    {
        return appendChild(new QDomLiteElement(name,map));
    }
    inline QDomLiteElement* appendChild(QDomLiteElement* element)
    {
        if (!element) return nullptr;
        childElements.append(element);
        return element;
    }
    inline QDomLiteElement* appendClone(const QDomLiteElement* element) { return appendChild(new QDomLiteElement(element)); }
    inline QDomLiteElement* appendChildFromString(const XMLStringClass& XML)
    {
        auto e=appendChild(new QDomLiteElement);
        e->fromString(XML);
        return e;
    }
    inline QDomLiteElement* prependChild(QDomLiteElement* element)
    {
        if (!element) return nullptr;
        childElements.prepend(element);
        return element;
    }
    inline QDomLiteElement* prependChild(const QString& name, const QString& attrName, const QDomLiteValue& attrValue)
    {
        return prependChild(new QDomLiteElement(name,attrName,attrValue));
    }
    inline QDomLiteElement* prependChild(const QString& name, const QDomLiteNameList& names, const QDomLiteValueList& values)
    {
        return prependChild(new QDomLiteElement(name,names,values));
    }
    inline QDomLiteElement* prependChild(const QString& name, const QDomLiteAttributeMap& map)
    {
        return prependChild(new QDomLiteElement(name,map));
    }
    inline QDomLiteElement* prependClone(const QDomLiteElement* element) { return prependChild(new QDomLiteElement(element)); }
    inline QDomLiteElement* prependChild(const QString& name) { return prependChild(new QDomLiteElement(name)); }
    inline QDomLiteElement* insertChild(QDomLiteElement* element, const int insertBefore)
    {
        if (!element) return nullptr;
        if ((insertBefore > -1) && (insertBefore < childElements.size()))
        {
            childElements.insert(childElements.constBegin() + insertBefore,element);
        }
        else
        {
            childElements.append(element);
        }
        return element;
    }
    inline QDomLiteElement* insertChild(QDomLiteElement* element, QDomLiteElement* insertBefore) { return insertChild(element,childElements.indexOf(insertBefore)); }
    inline QDomLiteElement* insertChild(const QString& name, const int insertBefore) { return insertChild(new QDomLiteElement(name),insertBefore); }
    inline QDomLiteElement* insertChild(const QString& name, QDomLiteElement* insertBefore) { return insertChild(new QDomLiteElement(name),insertBefore); }
    inline QDomLiteElement* insertClone(const QDomLiteElement* element, const int insertBefore) { return insertChild(new QDomLiteElement(element),insertBefore); }
    inline QDomLiteElement* insertClone(const QDomLiteElement* element, QDomLiteElement* insertBefore) { return insertChild(new QDomLiteElement(element),insertBefore); }
    inline void swapChild(const int index, QDomLiteElement** element)
    {
        if (!elementExists(index)) return;
        QDomLite::swapElements(&childElements[index],element);
    }
    inline void swapChild(const QString& name, QDomLiteElement** element)
    {
        auto elem=elementByTag(name);
        if (elem != nullptr) swapChild(elem,element);
    }
    inline void swapChild(QDomLiteElement* childElement, QDomLiteElement** element)
    {
        swapChild(childElements.indexOf(childElement),element);
    }
    inline void appendChildren(QDomLiteElementList& elements) { childElements.append(elements); }
    inline void insertChildren(QDomLiteElementList& elements, int insertBefore)
    {
        for (auto e : elements)
        {
            if ((insertBefore > -1) && (insertBefore < childElements.size()))
            {
                childElements.insert(childElements.constBegin() + insertBefore++,e);
            }
            else
            {
                childElements.append(e);
            }
        }
    }
    inline void insertChildren(QDomLiteElementList& elements, QDomLiteElement* insertBefore) { insertChildren(elements, elements.indexOf(insertBefore)); }
    inline void removeChildren(QDomLiteElementList& elements)
    {
        for (auto e : elements) takeChild(e);
        qDeleteAll(elements);
        elements.clear();
    }
    inline void removeChildren(const QString& name)
    {
        auto elements=elementsByTag(name);
        removeChildren(elements);
    }
    inline QDomLiteElementList takeChildren(QDomLiteElementList& elements)
    {
        QDomLiteElementList RetVal;
        for (auto e : elements) RetVal.append(takeChild(e));
        return RetVal;
    }
    inline QDomLiteElementList takeChildren(const QString& name)
    {
        auto elements=elementsByTag(name);
        return takeChildren(elements);
    }
    inline int childCount() const { return childElements.size(); }
    inline int childCount(const QString& name) const {
        int count=0;
        for (const auto e : childElements) if (e->matches(name)) count++;
        return count;
    }
    inline QDomLiteElement* childElement(const int index) const {
        return (!elementExists(index)) ? nullptr : childElements.at(index);
    }
    inline QDomLiteElement* firstChild() const { return childElement(0); }
    inline QDomLiteElement* lastChild() const { return childElement(childElements.size()-1); }
    int inline indexOfChild(QDomLiteElement* element) const { return childElements.indexOf(element); }
    inline QDomLiteElement* clone() const { return new QDomLiteElement(this); }
    inline void copy(const QDomLiteElement* other)
    {
        clear();
        tag=other->tag;
        text=other->text;
        CDATA=other->CDATA;
        comments.append(other->comments);
        for (const auto a : other->attributes) attributes.append(a->clone());
        for (const auto e : other->childElements) childElements.append(e->clone());
    }
    inline const QString toString(const int indentLevel=-1) const
    {
        const QString Indent(indentLevel,QChar::Tabulation);
        if (!CDATA.isEmpty()) return Indent+QStringLiteral("<![CDATA[")+CDATA+QStringLiteral("]]>\n");
        QString RetVal;
        for (const QDomLiteValue& c : comments) RetVal+=Indent+QStringLiteral("<!--")+c.encodedString()+QStringLiteral("-->\n");
        RetVal+=Indent+'<'+tag+attributesString();
        if (!text.isEmpty())
        {
            RetVal+='>'+ text.encodedString()+QStringLiteral("</")+tag+QStringLiteral(">\n");
        }
        else if (!childElements.isEmpty())
        {
            RetVal+=QStringLiteral(">\n");
            for (const auto e : childElements) RetVal += e->toString(indent(indentLevel));
            RetVal+=Indent+QStringLiteral("</")+tag+QStringLiteral(">\n");
        }
        else
        {
            RetVal+=QStringLiteral("/>\n");
        }
        return RetVal;
    }
    inline int fromString(const XMLStringClass& XML, int start=0)
    {
#ifdef QT_DEBUG
        if (rxOther.matchView(XML.sliced(start,qMin(20,XML.length()-start))).capturedStart()==0)
#else
        //if (rxOther.matchView(XML.sliced(start,qMin(20,XML.length()-start))).capturedStart()==0)
        if (rxOther.matchView(XML.sliced(start,20)).capturedStart()==0)
#endif
        {
            forever // comment
            {
                const auto RemarkMatch = rxRemark.matchView(XML,start);
                if (RemarkMatch.capturedStart()!=start) break;
                start+=RemarkMatch.capturedLength();
                comments.append(QDomLite::valueFromString(RemarkMatch.captured(1)));
            }
            const auto CDATAMatch = rxCDATA.matchView(XML,start);
            if (CDATAMatch.capturedStart()==start)
            {
                CDATA=CDATAMatch.captured(1);
                return start+CDATAMatch.capturedLength();
            }
        }
        const auto TagMatch = rxTag.matchView(XML,start);
        if (TagMatch.capturedStart() == start)
        {
            tag=TagMatch.captured(1);
            const QString Attr(TagMatch.captured(2));
            start+=TagMatch.capturedLength();
            if (!Attr.endsWith('/')) //element must have an end tag
            {
                const QRegularExpression rxEndTag(QStringLiteral("\\s*</")+tag+QStringLiteral(">\\s*")); //end tag
                const QRegularExpression rxSameTag(QStringLiteral("<")+tag);
                const auto EndTagMatch = rxEndTag.matchView(XML,start);
                const int EndTagPtr = EndTagMatch.capturedStart();
                const int EndTagLen = EndTagMatch.capturedLength();
                const int childLen = EndTagPtr - start;
#ifdef QT_DEBUG
                const XMLStringClass childString(XML.sliced(start, qMax(0,childLen)));
#else
                const XMLStringClass childString(XML.sliced(start, childLen));
#endif
                if (!rxSameTag.matchView(childString).hasMatch()) // no nested tags
                {
                    int childStart = 0;
                    while (childStart < childLen)
                    {
                        const int i = childStart;
                        auto e = new QDomLiteElement(childString, childStart);
                        if (i == childStart)
                        {
                            delete e;
                            if (childStart == 0) {
                                text.fromEncodedString(childString); // it´s a text element
                            }
                            break;
                        }
                        childElements.append(e);
                    }
                    start = EndTagPtr + EndTagLen; // use end tag found
                }
                else // element has nested tags
                {
                    forever
                    {
                        const int i = start;
                        auto e = new QDomLiteElement(XML, start);
                        if (i == start)
                        {
                            delete e;
                            break;
                        }
                        childElements.append(e);
                    }
#ifdef QT_DEBUG
                    const auto EndTagMatch = rxEndTag.matchView(XML.sliced(start,qMin(XMLendtaglen,XML.length()-start)));
#else
                    //const auto EndTagMatch = rxEndTag.matchView(XML.sliced(start,qMin(XMLendtaglen,XML.length()-start)));
                    const auto EndTagMatch = rxEndTag.matchView(XML.sliced(start,XMLendtaglen));
#endif
                    if (EndTagMatch.capturedStart()==0) // look for new end tag
                    {
                        start+=EndTagMatch.capturedLength();
                    }
                }
            }
            appendAttributesString(Attr);
        }
        return start;
    }
    inline void clear()
    {
        tag.clear();
        text.clear();
        CDATA.clear();
        clearChildren();
        comments.clear();
        clearAttributes();
    }
    inline void clear(const QString& Tag)
    {
        clear();
        tag=Tag;
    }
    inline void clearChildren()
    {
        qDeleteAll(childElements);
        childElements.clear();
    }
    inline bool compare(const QDomLiteElement* element) const
    {
        if (!element) return false;
        if (!matches(element->tag)) return false;
        if (attributeCount() != element->attributeCount()) return false;
        if (toString() != element->toString()) return false;
        if (childCount() != element->childCount()) return false;
        for (int i = 0; i < childElements.size(); i++) if (!childElements.at(i)->compare(element->childElements.at(i))) return false;
        return true;
    }
    inline bool compare(const XMLStringClass& XML) const
    {
        QDomLiteElement e;
        e.fromString(XML);
        return compare(&e);
    }
    inline void mergeWith(QDomLiteElement* element) {
        if (element) {
            for (const auto a : std::as_const(element->attributes)) attributes.append(a->clone());
            for (const auto e : std::as_const(element->childElements)) childElements.append(e->clone());
            delete element;
        }
    }
    inline void mergeWithClone(QDomLiteElement* element) {
        if (element) {
            for (const auto a : std::as_const(element->attributes)) attributes.append(a->clone());
            for (const auto e : std::as_const(element->childElements)) childElements.append(e->clone());
        }
    }
    inline operator QString() { return toString(0); }
    inline void operator = (QDomLiteElement& other) { copy(&other); }
    inline void operator + (QDomLiteElement& other) { mergeWithClone(&other); }
    inline QDomLiteElement& operator += (QDomLiteElement& other) {
        mergeWithClone(&other);
        return *this;
    }
    inline bool operator == (QDomLiteElement& element) { return compare(&element); }
    inline bool operator == (const XMLStringClass& s) { return compare(s); }
    inline bool operator != (QDomLiteElement& element) { return !compare(&element); }
    inline bool operator != (const XMLStringClass& s) { return !compare(s); }
    inline QDomLiteElement& operator << (QDomLiteElement& element) {
        appendChild(&element);
        return *this;
    }
    inline QDomLiteElement& operator << (QDomLiteElementList& elements) {
        appendChildren(elements);
        return *this;
    }
    inline QDomLiteElement& operator << (const QString& name) {
        appendChild(name);
        return *this;
    }
    bool inline matches(const QString& Tag) const { return (tag==Tag); }
private:
    inline int indent(const int indentLevel=-1) const
    {
        return (indentLevel>-1) ? indentLevel+1 : -1;
    }
    inline bool elementExists(const int index) const { return ((index < childElements.size()) && (index >= 0)); }
};

class QDomLiteDocument : public QDomLiteAttributes
{
public:
    inline QDomLiteDocument(const QString& docType, const QString& docTag)
    {
        this->docType=docType;
        documentElement=new QDomLiteElement(docTag);
    }
    inline QDomLiteDocument(const QString& path) {
        documentElement=new QDomLiteElement;
        load(path);
    }
    inline QDomLiteDocument(const QDomLiteDocument* other) {
        documentElement=new QDomLiteElement;
        copy(other);
    }
    inline QDomLiteDocument(const QString& path,const QString& defaultType, const QString& defaultTag) {
        docType=defaultType;
        documentElement=new QDomLiteElement(defaultTag);
        if (QFileInfo::exists(path)) load(path);
    }
    inline QDomLiteDocument(QIODevice& file) {
        documentElement=new QDomLiteElement;
        fromFile(file);
    }
    inline QDomLiteDocument(QByteArray& byteArray) {
        documentElement=new QDomLiteElement;
        fromByteArray(byteArray);
    }
    inline QDomLiteDocument() { documentElement=new QDomLiteElement; }
    inline ~QDomLiteDocument()
    {
        delete documentElement;
    }
    inline bool fromFile(QIODevice& file) {
        if (file.isOpen())
        {
            fromByteArray(file.readAll());
            return true;
        }
        else
        {
            if (file.open(QIODevice::ReadOnly))
            {
                fromByteArray(file.readAll());
                file.close();
                return true;
            }
        }
        return false;
    }
    inline void fromByteArray(const QByteArray& byteArray) {
        fromString(decodedByteArray(byteArray));
    }
    QDomLiteElement* documentElement;
    inline QDomLiteElement* replaceDoc(QDomLiteElement* element)
    {
        delete documentElement;
        documentElement = element;
        return element;
    }
    inline QDomLiteElement* exchangeDoc(QDomLiteElement* element)
    {
        QDomLiteElement** t = &documentElement;
        documentElement = element;
        return *t;
    }
    inline void swapDoc(QDomLiteElement** element)
    {
        QDomLite::swapElements(&documentElement,element);
    }
    inline bool save(const QString& path, const bool indent = false)
    {
        QFile file(path);
        if(!file.open(QIODevice::WriteOnly)) return false;
        QTextStream ts( &file );
        ts << toString(indent);
        file.close();
        return true;
    }
    inline QByteArray toByteArray(const bool indent = false)
    {
        QByteArray b;
        QTextStream ts(&b);
        ts << toString(indent);
        return b;
    }
    inline bool load(const QString& path)
    {
        QFile fileData(path);
        return fromFile(fileData);
    }
    inline void clear()
    {
        docType.clear();
        comments.clear();
        entities.clear();
        documentElement->clear();
        clearAttributes();
    }
    inline void clear(const QString& docType, const QString& docTag)
    {
        clear();
        this->docType=docType;
        documentElement->tag=docTag;
    }
    inline void fromString(const XMLStringClass& XML)
    {
        clear();
        int Ptr = 0;
        while (appendComments(XML, Ptr)){}
        const auto DocTypeMatch = rxdocType.matchView(XML, Ptr);
        if (DocTypeMatch.capturedStart() == Ptr)
        {
            docType=DocTypeMatch.captured(1);
            Ptr+=DocTypeMatch.capturedLength();
            const auto EntityMatch = rxEntityTags.matchView(XML, Ptr - 1);
            if (EntityMatch.capturedStart() == Ptr - 1)
            {
                const QString ent(EntityMatch.captured(1));
                int eptr = 0;
                while (appendEntities(ent, eptr)){}
                Ptr+=EntityMatch.capturedLength();
            }
        }
        while (appendComments(XML, Ptr)){}
        documentElement->fromString(XML, Ptr);
    }
    inline QDomLiteDocument* clone() const { return new QDomLiteDocument(this); }
    inline void copy(const QDomLiteDocument* other)
    {
        clear();
        docType=other->docType;
        comments=other->comments;
        entities=other->entities;
        for (const auto a : other->attributes) attributes.append(a->clone());
        replaceDoc(other->documentElement->clone());
    }
    inline const QString toString(const bool indent=false) const
    {
        QString RetVal = (!attributes.isEmpty()) ? QStringLiteral("<?xml") + attributesString() + QStringLiteral("?>\n") :
                                                   QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
        if (!docType.isEmpty())
        {
            RetVal += QStringLiteral("<!DOCTYPE ")+docType;
            if (!entities.isEmpty())
            {
                RetVal+=QStringLiteral(" [\n");
                for (auto it = entities.constKeyValueBegin(); it != entities.constKeyValueEnd(); it++)
                {
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
                    RetVal+=QStringLiteral("<!ENTITY ")+it->first.midRef(1,it->first.size()-2)+QStringLiteral(" \"")+it->second+QStringLiteral("\">\n");
#else
                    RetVal+=QStringLiteral("<!ENTITY ")+it->first.mid(1,it->first.size()-2)+QStringLiteral(" \"")+it->second+QStringLiteral("\">\n");
#endif
                }
                RetVal+=QStringLiteral("] ");
            }
            RetVal+=QStringLiteral(">\n");
        }
        for (const QDomLiteValue& c : comments) RetVal += QStringLiteral("<!-- ")+c.encodedString()+QStringLiteral("-->\n");
        return RetVal + documentElement->toString(-(!indent));
    }
    inline const QString decodeEntities(QDomLiteElement* textElement) const
    {
        return decodeEntities(textElement->text);
    }
    inline const QString decodeEntities(const QString& text) const
    {
        if (entities.isEmpty()) return text;
        QString retVal(text);
        for (auto it = entities.constKeyValueBegin(); it != entities.constKeyValueEnd(); it++) retVal.replace(it->first,it->second);
        return retVal;
    }
    inline void addEntity(const QString& entity, const QString& value)
    {
        QString e = entity;
        if (!e.startsWith('&')) e = '&' + e;
        if (!e.endsWith(';')) e += ';';
        entities.insert(e, value);
    }
    QString docType;
    QDomLiteValueList comments;
    QDomLiteEntityMap entities;
    inline operator QString() { return toString(true); }
private:
    const char *UTF_16_BE_BOM = "\xFE\xFF";
    const char *UTF_16_LE_BOM = "\xFF\xFE";
    const char *UTF_8_BOM = "\xEF\xBB\xBF";
    const char *UTF_32_BE_BOM = "\x00\x00\xFE\xFF";
    const char *UTF_32_LE_BOM = "\xFF\xFE\x00\x00";
    QString check_bom(const char *data, size_t size)
    {
        if (size >= 3) {
            if (memcmp(data, UTF_8_BOM, 3) == 0)
                return "UTF-8";
        }
        if (size >= 4) {
            if (memcmp(data, UTF_32_LE_BOM, 4) == 0)
                return "UTF-32-LE";
            if (memcmp(data, UTF_32_BE_BOM, 4) == 0)
                return "UTF-32-BE";
        }
        if (size >= 2) {
            if (memcmp(data, UTF_16_LE_BOM, 2) == 0)
                return "UTF-16-LE";
            if (memcmp(data, UTF_16_BE_BOM, 2) == 0)
                return "UTF-16-BE";
        }
        return QString();
    }
    QString decodedByteArray(const QByteArray& a)
    {
        if (check_bom(a.constData(),4).contains("UTF-16"))
        {
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
            auto fromUtf16 = QStringEncoder(QStringEncoder::Utf8);
            return QString(fromUtf16(a));
#else
            return QString::fromUtf16(reinterpret_cast<const ushort*>(a.constData()));
#endif
        }
        return a;
    }
    inline bool appendEntities(const XMLStringClass& XML, int& Ptr)
    {
        bool retVal=false;
        forever
        {
            const auto RemarkMatch = rxRemark.matchView(XML, Ptr);
            if (RemarkMatch.capturedStart() != Ptr) break;
            Ptr += RemarkMatch.capturedLength(); //skip!
        }

        forever
        {
            const auto EntityMatch = rxEntity.matchView(XML, Ptr);
            if (EntityMatch.capturedStart() != Ptr) break;
            entities.insert('&' + EntityMatch.captured(1) + ';', EntityMatch.captured(2));
            Ptr += EntityMatch.capturedLength();
            retVal=true;
        }
        return retVal;
    }
    inline bool appendComments(const XMLStringClass& XML, int& Ptr)
    {
        bool retVal=false;
        forever
        {
#ifdef QT_DEBUG
            const auto XMLAttributesMatch = rxXMLAttributes.matchView(XML.sliced(Ptr,qMin(XMLmaxtaglen,XML.length()-Ptr)));
#else
            //const auto XMLAttributesMatch = rxXMLAttributes.matchView(XML.sliced(Ptr,qMin(XMLmaxtaglen,XML.length()-Ptr)));
            const auto XMLAttributesMatch = rxXMLAttributes.matchView(XML.sliced(Ptr,XMLmaxtaglen));
#endif
            if (XMLAttributesMatch.capturedStart() != 0) break;
            appendAttributesString(XMLAttributesMatch.captured(1));
            Ptr += XMLAttributesMatch.capturedLength();
            retVal=true;
        }
        forever
        {
            const auto RemarkMatch = rxRemark.matchView(XML, Ptr);
            if (RemarkMatch.capturedStart() != Ptr) break;
            comments.append(QDomLite::valueFromString(RemarkMatch.captured(1)));
            Ptr += RemarkMatch.capturedLength();
            retVal=true;
        }
        return retVal;
    }
};

namespace QDomLite
{
inline const QDomLiteElement elementFromString(const QString& s) {
    QDomLiteElement e;
    e.fromString(s);
    return e;
}
}

#pragma pack(pop)

#endif // QDOMLITE_H
