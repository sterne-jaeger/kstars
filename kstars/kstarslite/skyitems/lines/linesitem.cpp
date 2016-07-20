/** *************************************************************************
                          linesitem.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 1/06/2016
    copyright            : (C) 2016 by Artem Fedoskin
    email                : afedoskin3@gmail.com
 ***************************************************************************/
/** *************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "Options.h"
#include "projections/projector.h"
#include <QSGNode>

#include "linesitem.h"
#include "linelist.h"
#include "linelistindex.h"
#include "../skynodes/nodes/linenode.h"
#include "../skynodes/trixelnode.h"

LinesItem::LinesItem(RootNode *rootNode)
    :SkyItem(LabelsItem::label_t::NO_LABEL, rootNode)
{

}

void LinesItem::addLinesComponent(LineListIndex *linesComp, QString color, int width, Qt::PenStyle style) {
    LineIndexNode *node = new LineIndexNode;
    appendChildNode(node);

    m_lineIndexes.insert(node, linesComp);
    LineListHash *list = linesComp->lineIndex();
    //Sort by trixels
    QMap<Trixel, LineListList*> trixels;

    QHash< Trixel, LineListList *>::const_iterator s = list->begin();
    while(s != list->end()) {
        trixels.insert(s.key(), *s);
        s++;
    }

    QMap< Trixel, LineListList *>::const_iterator i = trixels.begin();
    QList<LineList *> addedLines;
    while( i != trixels.end()) {
        LineListList *linesList = *i;

        if(linesList->size()) {
            TrixelNode *trixel = new TrixelNode(i.key());
            node->appendChildNode(trixel);

            QColor schemeColor = KStarsData::Instance()->colorScheme()->colorNamed(color);
            for(int c = 0; c < linesList->size(); ++c) {
                LineList *list = linesList->at(c);
                if(!addedLines.contains(list)) {
                    trixel->appendChildNode(new LineNode(linesList->at(c), 0, schemeColor, width, style));
                    addedLines.append(list);
                }
            }
        }
        ++i;
    }
}

void LinesItem::update() {
    QMap< LineIndexNode *, LineListIndex *>::const_iterator i = m_lineIndexes.begin();

    //SkyMesh * mesh = SkyMesh::Instance();
    SkyMapLite *map = SkyMapLite::Instance();

    double radius = map->projector()->fov();
    if ( radius > 90.0 ) radius = 90.0;

    UpdateID updateID = KStarsData::Instance()->updateID();

    SkyPoint* focus = map->focus();
    //mesh->aperture( focus, radius + 1.0, DRAW_BUF ); // divide by 2 for testing
    //Doesn't work stable

//    MeshIterator region(mesh, DRAW_BUF);

    while( i != m_lineIndexes.end()) {
//        int count = 0;
        int regionID = -1;
        /*if(region.hasNext()) {
            regionID = region.next();
            count++;
        }*/

        LineIndexNode * node = i.key();
        if(i.value()->selected()) {
            node->show();

            QSGNode *n = node->firstChild();
            while(n != 0) {
                TrixelNode * trixel = static_cast<TrixelNode *>(n);
                //qDebug() << trixel->trixelID() << regionID;
                /*if(trixel->trixelID() < regionID) {
                    trixel->hide();
                } else if(trixel->trixelID() > regionID) {
                    if(region.hasNext()) {
                        regionID = region.next();
                        count++;
                    } else {
                        //Hide all trixels that has greater ID than regionID
                        regionID = mesh->size();
                    }
                    continue;
                } else {*/
                    trixel->show();

                    QSGNode *l = trixel->firstChild();
                    while(l != 0) {
                        LineNode * lines = static_cast<LineNode *>(l);
                        l = l->nextSibling();

                        LineList * lineList = lines->lineList();
                        if ( lineList->updateID != updateID )
                            i.value()->JITupdate( lineList );

                        lines->updateGeometry();
                    }
                //}
                n = n->nextSibling();
            }
        } else {
            node->hide();
        }
        ++i;
        //region.reset();
        //qDebug() << count;
    }
}

