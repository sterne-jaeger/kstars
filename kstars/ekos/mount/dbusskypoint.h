#ifndef DBUSSKYPOINT_H
#define DBUSSKYPOINT_H

struct DBusSkyPoint {
    double ra;
    double dec;
};

Q_DECLARE_METATYPE(DBusSkyPoint);

// Marshall the DBusSkyPoint data into a D-Bus argument
inline QDBusArgument &operator<<(QDBusArgument &argument, const DBusSkyPoint &point)
{
    argument.beginStructure();
    argument << point.ra << point.dec;
    argument.endStructure();
    return argument;
}

// Retrieve the DBusSkyPoint data from the D-Bus argument
inline const QDBusArgument &operator>>(const QDBusArgument &argument, DBusSkyPoint &point)
{
    argument.beginStructure();
    argument >> point.ra >> point.dec;
    argument.endStructure();
    return argument;
}

#endif // DBUSSKYPOINT_H
