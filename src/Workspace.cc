// -*- mode: C++; indent-tabs-mode: nil; -*-
// Workspace.cc for Blackbox - an X11 Window manager
// Copyright (c) 2001 - 2002 Sean 'Shaleh' Perry <shaleh@debian.org>
// Copyright (c) 1997 - 2000 Brad Hughes (bhughes@tcac.net)
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

#ifdef    HAVE_CONFIG_H
#  include "../config.h"
#endif // HAVE_CONFIG_H

extern "C" {
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#ifdef    HAVE_STDIO_H
#  include <stdio.h>
#endif // HAVE_STDIO_H

#ifdef HAVE_STRING_H
#  include <string.h>
#endif // HAVE_STRING_H
}

#include <functional>
#include <string>

using std::string;

#include "i18n.hh"
#include "blackbox.hh"
#include "Clientmenu.hh"
#include "Netizen.hh"
#include "Screen.hh"
#include "Toolbar.hh"
#include "Util.hh"
#include "Window.hh"
#include "Workspace.hh"
#include "Windowmenu.hh"


Workspace::Workspace(BScreen *scrn, unsigned int i) {
  screen = scrn;

  cascade_x = cascade_y = 32;

  id = i;

  clientmenu = new Clientmenu(this);

  lastfocus = (BlackboxWindow *) 0;

  setName(screen->getNameOfWorkspace(id));
}


void Workspace::addWindow(BlackboxWindow *w, Bool place) {
  assert(w != 0);

  if (place) placeWindow(w);

  w->setWorkspace(id);
  w->setWindowNumber(windowList.size());

  stackingList.push_front(w);
  windowList.push_back(w);

  clientmenu->insert(w->getTitle());
  clientmenu->update();

  screen->updateNetizenWindowAdd(w->getClientWindow(), id);

  raiseWindow(w);
}


unsigned int Workspace::removeWindow(BlackboxWindow *w) {
  assert(w != 0);

  stackingList.remove(w);

  if (w->isFocused()) {
    BlackboxWindow *newfocus = 0;
    if (w->isTransient()) newfocus = w->getTransientFor();
    if (! newfocus && ! stackingList.empty()) newfocus = stackingList.front();
    if (! newfocus || ! newfocus->setInputFocus()) {
      screen->getBlackbox()->setFocusedWindow(0);
    }
  }

  if (lastfocus == w)
    lastfocus = (BlackboxWindow *) 0;

  windowList.remove(w);
  clientmenu->remove(w->getWindowNumber());
  clientmenu->update();

  screen->updateNetizenWindowDel(w->getClientWindow());

  BlackboxWindowList::iterator it = windowList.begin();
  const BlackboxWindowList::iterator end = windowList.end();
  unsigned int i = 0;
  for (; it != end; ++it, ++i)
    (*it)->setWindowNumber(i);

  if (i == 0)
    cascade_x = cascade_y = 32;

  return i;
}


void Workspace::showAll(void) {
  std::for_each(stackingList.begin(), stackingList.end(),
                std::mem_fun(&BlackboxWindow::show));
}


void Workspace::hideAll(void) {
  // withdraw in reverse order to minimize the number of Expose events

  BlackboxWindowList lst(stackingList.rbegin(), stackingList.rend());

  BlackboxWindowList::iterator it = lst.begin();
  const BlackboxWindowList::iterator end = lst.end();
  for (; it != end; ++it) {
    BlackboxWindow *bw = *it;
    if (! bw->isStuck())
      bw->withdraw();
  }
}


void Workspace::removeAll(void) {
  while (! windowList.empty())
    windowList.front()->iconify();
}


void Workspace::raiseWindow(BlackboxWindow *w) {
  BlackboxWindow *win = (BlackboxWindow *) 0, *bottom = w;

  while (bottom->isTransient()) {
    BlackboxWindow *bw = bottom->getTransientFor();
    if (! bw) break;
    bottom = bw;
  }

  unsigned int i = 1;
  win = bottom;
  while ((win = win->getTransient())) ++i;

  Window *nstack = new Window[i], *curr = nstack;

  win = bottom;
  while (True) {
    *(curr++) = win->getFrameWindow();
    screen->updateNetizenWindowRaise(win->getClientWindow());

    if (! win->isIconic()) {
      Workspace *wkspc = screen->getWorkspace(win->getWorkspaceNumber());
      wkspc->stackingList.remove(win);
      wkspc->stackingList.push_front(win);
    }

    win = win->getTransient();
    if (! win)
      break;
  }

  screen->raiseWindows(nstack, i);

  delete [] nstack;
}


void Workspace::lowerWindow(BlackboxWindow *w) {
  BlackboxWindow *win = (BlackboxWindow *) 0, *bottom = w;

  while (bottom->isTransient()) {
    BlackboxWindow *bw = bottom->getTransientFor();
    if (! bw) break;
    bottom = bw;
  }

  unsigned int i = 1;
  win = bottom;
  while (win->getTransient()) {
    win = win->getTransient();
    ++i;
  }

  Window *nstack = new Window[i], *curr = nstack;

  while (True) {
    *(curr++) = win->getFrameWindow();
    screen->updateNetizenWindowLower(win->getClientWindow());

    if (! win->isIconic()) {
      Workspace *wkspc = screen->getWorkspace(win->getWorkspaceNumber());
      wkspc->stackingList.remove(win);
      wkspc->stackingList.push_back(win);
    }

    win = win->getTransientFor();
    if (! win)
      break;
  }

  XLowerWindow(screen->getBaseDisplay()->getXDisplay(), *nstack);
  XRestackWindows(screen->getBaseDisplay()->getXDisplay(), nstack, i);

  delete [] nstack;
}


void Workspace::reconfigure(void) {
  clientmenu->reconfigure();

  BlackboxWindowList::iterator it = windowList.begin();
  const BlackboxWindowList::iterator end = windowList.end();
  for (; it != end; ++it) {
    BlackboxWindow *bw = *it;
    if (bw->validateClient())
      bw->reconfigure();
  }
}


BlackboxWindow *Workspace::getWindow(unsigned int index) {
  if (index < windowList.size()) {
    BlackboxWindowList::iterator it = windowList.begin();
    for(; index > 0; --index, ++it); /* increment to index */
    return *it;
  }
  return 0;
}


BlackboxWindow*
Workspace::getNextWindowInList(BlackboxWindow *w) {
  BlackboxWindowList::iterator it = std::find(windowList.begin(),
                                              windowList.end(),
                                              w);
  assert(it != windowList.end());   // window must be in list
  ++it;                             // next window
  if (it == windowList.end())
    return windowList.front();      // if we walked off the end, wrap around

  return *it;
}


BlackboxWindow* Workspace::getPrevWindowInList(BlackboxWindow *w) {
  BlackboxWindowList::iterator it = std::find(windowList.begin(),
                                              windowList.end(),
                                              w);
  assert(it != windowList.end()); // window must be in list
  if (it == windowList.begin())
    return windowList.back();     // if we walked of the front, wrap around

  return *(--it);
}


BlackboxWindow* Workspace::getTopWindowOnStack(void) const {
  return stackingList.front();
}


void Workspace::sendWindowList(Netizen &n) {
  BlackboxWindowList::iterator it = windowList.begin(),
    end = windowList.end();
  for(; it != end; ++it)
    n.sendWindowAdd((*it)->getClientWindow(), getID());
}


unsigned int Workspace::getCount(void) const {
  return windowList.size();
}


Bool Workspace::isCurrent(void) const {
  return (id == screen->getCurrentWorkspaceID());
}


Bool Workspace::isLastWindow(const BlackboxWindow* const w) const {
  return (w == windowList.back());
}

void Workspace::setCurrent(void) {
  screen->changeWorkspaceID(id);
}


void Workspace::setName(const string& new_name) {
  if (! new_name.empty()) {
    name = new_name;
  } else {
    string tmp =i18n(WorkspaceSet, WorkspaceDefaultNameFormat, "Workspace %d");
    assert(tmp.length() < 32);
    char default_name[32];
    sprintf(default_name, tmp.c_str(), id + 1);
    name = default_name;
  }

  clientmenu->setLabel(name);
  clientmenu->update();
}


typedef std::vector<Rect> rectList;

static rectList calcSpace(const Rect &win, const rectList &spaces) {
  rectList result;
  rectList::const_iterator siter, end = spaces.end();
  for(siter = spaces.begin(); siter != end; ++siter) {
    const Rect& curr = *siter;
    if(! win.intersects(curr)) {
      result.push_back(curr);
      continue;
    }
    //Check for space to the left of the window
    if(win.x() > curr.x())
      result.push_back(Rect(curr.x(), curr.y(),
                            win.x() - curr.x() - 1,
                            curr.height()));
    //Check for space above the window
    if(win.y() > curr.y())
      result.push_back(Rect(curr.x(), curr.y(),
                            curr.width(),
                            win.y() - curr.y() - 1));
    //Check for space to the right of the window
    if(win.right() < curr.right())
      result.push_back(Rect(win.right() + 1, curr.y(),
                            curr.right() - win.right() - 1, curr.height()));
    //Check for space below the window
    if(win.bottom() < curr.bottom())
      result.push_back(Rect(curr.x(), win.bottom() + 1,
                            curr.width(), curr.bottom() - win.bottom() - 1));
  }
  return result;
}


static Bool rowRLBT(const Rect &first, const Rect &second) {
  if (first.bottom() == second.bottom())
    return first.right() > second.right();
  return first.bottom() > second.bottom();
}
 
static Bool rowRLTB(const Rect &first, const Rect &second) {
  if (first.y() == second.y())
    return first.right() > second.right();
  return first.y() < second.y();
}

static Bool rowLRBT(const Rect &first, const Rect &second) {
  if (first.bottom() == second.bottom())
    return first.x() < second.x();
  return first.bottom() > second.bottom();
}
 
static Bool rowLRTB(const Rect &first, const Rect &second) {
  if (first.y() == second.y())
    return first.x() < second.x();
  return first.y() < second.y();
}
 
static Bool colLRTB(const Rect &first, const Rect &second) {
  if (first.x() == second.x())
    return first.y() < second.y();
  return first.x() < second.y();
}
 
static Bool colLRBT(const Rect &first, const Rect &second) {
  if (first.x() == second.x())
    return first.bottom() > second.bottom();
  return first.x() < second.y();
}

static Bool colRLTB(const Rect &first, const Rect &second) {
  if (first.right() == second.right())
    return first.y() < second.y();
  return first.right() > second.right();
}
 
static Bool colRLBT(const Rect &first, const Rect &second) {
  if (first.right() == second.right())
    return first.bottom() > second.bottom();
  return first.right() > second.right();
}


Bool Workspace::smartPlacement(Rect& win, const Rect& availableArea) {
  rectList spaces;
  spaces.push_back(availableArea); //initially the entire screen is free
  
  //Find Free Spaces
  BlackboxWindowList::iterator wit = windowList.begin(),
    end = windowList.end();
  for (; wit != end; ++wit) {
    const BlackboxWindow* const curr = *wit;
    Rect tmp(curr->getXFrame(), curr->getYFrame(),
             curr->getWidth() + (screen->getBorderWidth() * 4),
             ((curr->isShaded()) ?
              curr->getTitleHeight() :
              curr->getHeight()) + (screen->getBorderWidth() * 4));

    spaces = calcSpace(tmp, spaces);
  }

  if (screen->getPlacementPolicy() == BScreen::RowSmartPlacement) {
    if(screen->getRowPlacementDirection() == BScreen::LeftRight) {
      if(screen->getColPlacementDirection() == BScreen::TopBottom)
        sort(spaces.begin(), spaces.end(), rowLRTB);
      else
        sort(spaces.begin(), spaces.end(), rowLRBT);
    } else {
      if(screen->getColPlacementDirection() == BScreen::TopBottom)
        sort(spaces.begin(), spaces.end(), rowRLTB);
      else
        sort(spaces.begin(), spaces.end(), rowRLBT);
    }
  } else {
    if(screen->getColPlacementDirection() == BScreen::TopBottom) {
      if(screen->getRowPlacementDirection() == BScreen::LeftRight)
        sort(spaces.begin(), spaces.end(), colLRTB);
      else
        sort(spaces.begin(), spaces.end(), colRLTB);
    } else {
      if(screen->getRowPlacementDirection() == BScreen::LeftRight)
        sort(spaces.begin(), spaces.end(), colLRBT);
      else
        sort(spaces.begin(), spaces.end(), colRLBT);
    }
  }

  rectList::const_iterator sit = spaces.begin(), spaces_end = spaces.end();
  for(; sit != spaces_end; ++sit) {
    if (sit->width() >= win.width() && sit->height() >= win.height())
      break;
  }

  if (sit == spaces_end)
    return False;

  //set new position based on the empty space found
  const Rect& where = *sit;
  win.newX(where.x());
  win.newY(where.y());

  // adjust the location() based on left/right and top/bottom placement
  if (screen->getPlacementPolicy() == BScreen::RowSmartPlacement) {
    if (screen->getRowPlacementDirection() == BScreen::RightLeft)
      win.newX(where.right() - win.width());
    if (screen->getColPlacementDirection() == BScreen::BottomTop)
      win.newY(where.bottom() - win.height());
  } else {
    if (screen->getColPlacementDirection() == BScreen::BottomTop)
      win.newY(win.y() + where.height() - win.height());
    if (screen->getRowPlacementDirection() == BScreen::RightLeft)
      win.newX(win.x() + where.width() - win.width());
  }
  return True;
}


Bool Workspace::cascadePlacement(Rect &win, const Rect &availableArea) {
  if ((cascade_x > (availableArea.width() / 2)) ||
      (cascade_y > (availableArea.height() / 2)))
    cascade_x = cascade_y = 32;

  if (cascade_x == 32) {
    cascade_x += availableArea.x();
    cascade_y += availableArea.y();
  }

  win.newX(cascade_x);
  win.newY(cascade_y);

  return True;
}


void Workspace::placeWindow(BlackboxWindow *win) {
  Rect availableArea(screen->availableArea()),
    new_win(availableArea.x(), availableArea.y(),
            win->getWidth() + (screen->getBorderWidth() * 2),
            win->getHeight() + (screen->getBorderWidth() * 2));
  Bool placed = False;

  switch (screen->getPlacementPolicy()) {
  case BScreen::RowSmartPlacement:
  case BScreen::ColSmartPlacement:
    placed = smartPlacement(new_win, availableArea);
    break;
  default:
    break; // handled below
  } // switch

  if (placed == False) {
    cascadePlacement(new_win, availableArea);
    cascade_x += win->getTitleHeight() + (screen->getBorderWidth() * 2);
    cascade_y += win->getTitleHeight() + (screen->getBorderWidth() * 2);
  }

  if (new_win.right() > availableArea.right())
    new_win.newX((availableArea.right() - new_win.width()) / 2);
  if (new_win.bottom() > availableArea.bottom())
    new_win.newY((availableArea.bottom() - new_win.height()) / 2);

  win->configure(new_win.x(), new_win.y(), win->getWidth(), win->getHeight());
}
