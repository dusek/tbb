/*
    Copyright 2005-2009 Intel Corporation.  All Rights Reserved.

    This file is part of Threading Building Blocks.

    Threading Building Blocks is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.

    Threading Building Blocks is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Threading Building Blocks; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    As a special exception, you may use this file as part of a free software
    library without restriction.  Specifically, if other files instantiate
    templates or use macros or inline functions from this file, or you compile
    this file and link it with other files to produce an executable, this
    file does not by itself cause the resulting executable to be covered by
    the GNU General Public License.  This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
*/

/* 
    Game_of_life.cpp : 
                      main project file.
*/

#include <cstdlib>
#include "Board.h"
#include "Form1.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


Board::Board(int width, int height, int squareSize, Label^ counter)
: m_width(width), m_height(height), m_squareSize(squareSize), m_counter(counter)
{
    InitializeComponent();
    DoubleBuffered = true;

    this->Width = m_squareSize*width;
    this->Height = m_squareSize*height;
    
    m_matrix = new Matrix();
    m_matrix->width = width;
    m_matrix->height = height;
    m_matrix->data = new char[width*height];
    memset(m_matrix->data, 0, width*height);

    m_occupiedBrush = gcnew SolidBrush(Color::Black);
    m_freeBrush = gcnew SolidBrush(Color::LightGray);
    
    m_graphics = CreateGraphics();
    m_bmp = gcnew Bitmap(Width, Height);
    m_mem_dc = Graphics::FromImage(m_bmp);
}

void Board::seed(int s)
{        
    srand(s);
    for (int j=0; j<m_height; j++)
    {
        for (int i=0; i<m_width; i++)
        {        
            int x = rand()/(int)(((unsigned)RAND_MAX + 1) / 100);
            m_matrix->data[i+j*m_width] = x>75? 1: 0;               // 25% occupied
        }
    }
    
    Invalidate();
}

void Board::seed( const Board^ src )
{        
    for (int j=0; j<m_height; j++)
        for (int i=0; i<m_width; i++)
            memcpy(m_matrix->data, src->m_matrix->data, m_height*m_width);

    Invalidate();
}

void Board::draw(Graphics^ g)
{
    m_mem_dc->FillRectangle(m_freeBrush, Drawing::Rectangle(0, 0, m_width*m_squareSize, m_height*m_squareSize));
    for (int j=0; j<m_height; j++)
    {
        for (int i=0; i<m_width; i++)
        {    
            if ( m_matrix->data[i+j*m_width] )
            {
                m_mem_dc->FillRectangle(m_occupiedBrush, Drawing::Rectangle(i*m_squareSize, j*m_squareSize, m_squareSize, m_squareSize));
            }
        }
    }
    g->DrawImage(m_bmp, 0, 0);
}

void Board::OnPaint(PaintEventArgs^ e)
{
    draw(e->Graphics);
}

[STAThreadAttribute]
int main(array<System::String ^> ^args)
{
    // Enabling Windows XP visual effects before any controls are created
    Application::EnableVisualStyles();
    Application::SetCompatibleTextRenderingDefault(false); 

    // Create the main window and run it
    Application::Run(gcnew Form1());
    return 0;
}
