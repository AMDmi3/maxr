/***************************************************************************
 *      Mechanized Assault and Exploration Reloaded Projectfile            *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "combobox.h"
#include "listview.h"
#include "checkbox.h"
#include "lineedit.h"
#include "special/textlistviewitem.h"
#include "../../../utility/drawing.h"
#include "../../../utility/color.h"

//------------------------------------------------------------------------------
cComboBox::cComboBox (const cBox<cPosition>& area) :
	cWidget (area),
	maxVisibleItems (5)
{
	const cBox<cPosition> listViewArea (cPosition (area.getMinCorner ().x (), area.getMaxCorner ().y ()), cPosition (area.getMaxCorner ().x (), area.getMaxCorner ().y () + 5));
	listView = addChild (std::make_unique<cListView<cTextListViewItem>> (listViewArea));
	listView->setBeginMargin (cPosition (2, 2));
	listView->setEndMargin (cPosition (2, 0));
	listView->setItemDistance (cPosition (0, 0));
	listView->hide ();
	listView->disable ();

	downButton = addChild (std::make_unique<cCheckBox> (getEndPosition (), eCheckBoxType::ArrowDownSmall));
	downButton->move (downButton->getSize () * -1);

	const cBox<cPosition> lineEditArea (getPosition () + cPosition (2, (area.getSize ().y ()-font->getFontHeight (FONT_LATIN_NORMAL))/2)+1, cPosition (getEndPosition ().x() - downButton->getSize ().x () - 2, font->getFontHeight (FONT_LATIN_NORMAL)));
	lineEdit = addChild (std::make_unique<cLineEdit> (lineEditArea));
	lineEdit->setReadOnly (true);

	signalConnectionManager.connect (downButton->toggled, [this, area]()
	{
		if (downButton->isChecked ())
		{
			listView->show ();
			listView->enable ();
			fitToChildren ();
		}
		else
		{
			listView->hide ();
			listView->disable ();
			resize (area.getSize ());
		}
	});

	signalConnectionManager.connect (listView->itemClicked, [this](cTextListViewItem&)
	{
		downButton->setChecked (false);
	});

	signalConnectionManager.connect (listView->selectionChanged, [this]()
	{
		const auto selectedItem = listView->getSelectedItem ();
		if (selectedItem)
		{
			lineEdit->setText (selectedItem->getText ());
		}
	});

	signalConnectionManager.connect (listView->itemAdded, std::bind (&cComboBox::updateListViewSize, this));
	signalConnectionManager.connect (listView->itemRemoved, std::bind (&cComboBox::updateListViewSize, this));

	updateListViewSize ();
	updateLineEditBackground ();
}


//------------------------------------------------------------------------------
void cComboBox::draw ()
{
	if (!listView->isHidden() && listViewBackground != nullptr)
	{
		SDL_Rect position = listView->getArea ().toSdlRect ();
		SDL_BlitSurface (listViewBackground.get (), nullptr, cVideo::buffer, &position);
	}
	if (lineEditBackground != nullptr)
	{
		SDL_Rect position = getArea ().toSdlRect ();
		position.w = lineEditBackground->w;
		position.h = lineEditBackground->h;
		SDL_BlitSurface (lineEditBackground.get (), nullptr, cVideo::buffer, &position);
	}

	cWidget::draw ();
}

//------------------------------------------------------------------------------
void cComboBox::setMaxVisibleItems (size_t count)
{
	maxVisibleItems = count;
	updateListViewSize ();
}

//------------------------------------------------------------------------------
void cComboBox::updateListViewSize ()
{
	const auto visibleItems = static_cast<int>(std::min (maxVisibleItems, listView->getItemsCount ()));

	const auto itemHeight = font->getFontHeight (FONT_LATIN_NORMAL)+1;

	const auto requiredSize = listView->getBeginMargin ().y () + listView->getEndMargin ().y () + itemHeight*visibleItems + (visibleItems > 0 ? (listView->getItemDistance ().y ()*(visibleItems-1)) : 0) + 1;
	
	const auto currentSize = listView->getSize ();

	if (currentSize.y () != requiredSize)
	{
		listView->resize (cPosition (currentSize.x (), requiredSize));
		updateListViewBackground ();
	}
}

//------------------------------------------------------------------------------
void cComboBox::updateLineEditBackground ()
{
    lineEditBackground = AutoSurface (SDL_CreateRGBSurface (0, getSize ().x () - downButton->getSize ().x (), downButton->getSize ().y (), Video.getColDepth (), 0, 0, 0, 0));

	SDL_FillRect (lineEditBackground.get (), nullptr, 0x181818);

	drawRectangle (listViewBackground.get (), cBox<cPosition> (cPosition (0, 0), cPosition (lineEditBackground->w, lineEditBackground->h)), cColor::black());
}

//------------------------------------------------------------------------------
void cComboBox::updateListViewBackground ()
{
	const auto size = listView->getSize ();
	
    listViewBackground = AutoSurface (SDL_CreateRGBSurface (0, size.x (), size.y (), Video.getColDepth (), 0, 0, 0, 0));

	SDL_FillRect (listViewBackground.get (), nullptr, 0x181818);

	drawRectangle (listViewBackground.get (), cBox<cPosition> (cPosition (0, 0), size), cColor::black ());
}

//------------------------------------------------------------------------------
void cComboBox::addItem (std::string text)
{
	listView->addItem (std::make_unique<cTextListViewItem> (text));
}

//------------------------------------------------------------------------------
void cComboBox::removeItem (size_t index)
{
	listView->removeItem (listView->getItem (index));
}

//------------------------------------------------------------------------------
size_t cComboBox::getItemsCount () const
{
	return listView->getItemsCount ();
}

//------------------------------------------------------------------------------
const std::string& cComboBox::getItem (size_t index) const
{
	return listView->getItem (index).getText();
}

//------------------------------------------------------------------------------
void cComboBox::clearItems ()
{
	listView->clearItems ();
}

//------------------------------------------------------------------------------
const std::string& cComboBox::getSelectedText () const
{
	return lineEdit->getText();
}

//------------------------------------------------------------------------------
void cComboBox::setSelectedIndex (size_t index)
{
	listView->setSelectedItem (&listView->getItem (index));
}