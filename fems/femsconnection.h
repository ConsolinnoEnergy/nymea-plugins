#ifndef FEMSCONNECTION_H
#define FEMSCONNECTION_H

#include <QObject>

class FemsConnection : public QObject
{
    Q_OBJECT
public:
    explicit FemsConnection(QObject *parent = nullptr);

signals:

};

#endif // FEMSCONNECTION_H
