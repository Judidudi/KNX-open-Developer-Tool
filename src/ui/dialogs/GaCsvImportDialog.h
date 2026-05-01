#pragma once

#include "GroupAddress.h"
#include <QDialog>
#include <QList>

class QTableWidget;
class QLabel;
class QPushButton;

// Imports group addresses from an ETS CSV export.
// Accepted format (semicolon-separated, UTF-8 or Windows-1252):
//   "Group name";"Address";"Central";"Unfiltered";"Description";"Comment";"DPT";"DatapointSubtype";"...
// The dialog parses the file, shows a preview, and returns the parsed addresses.
class GaCsvImportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GaCsvImportDialog(const QString &filePath, QWidget *parent = nullptr);

    // Valid addresses parsed from the file.
    QList<GroupAddress> addresses() const { return m_addresses; }

private:
    void parse(const QString &filePath);

    QList<GroupAddress> m_addresses;
    QTableWidget       *m_preview  = nullptr;
    QLabel             *m_summary  = nullptr;
};
