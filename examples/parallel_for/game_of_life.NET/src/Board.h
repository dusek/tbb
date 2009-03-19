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

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace System;
using namespace System::ComponentModel;
using namespace System::Collections;
using namespace System::Windows::Forms;
using namespace System::Data;
using namespace System::Drawing;

struct Matrix 
{
    int width;
    int height;
    char* data;
};

    public ref class Board : public System::Windows::Forms::UserControl
    {
    public:
        Board(int width, int height, int squareSize, Label^ counter);        
        virtual ~Board()
        {
            if (components)
            {
                delete components;
            }
        }

        void seed(int s);
        void seed(const Board^ s);
    
    protected: 
        virtual void OnPaint(PaintEventArgs^ e) override;        
        void Board::draw(Graphics^ g);

    private:
        System::ComponentModel::Container ^components;

#pragma region Windows Form Designer generated code
        void InitializeComponent(void)
        {
            this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
        }
#pragma endregion

    private: delegate void drawDelegate(Int32);
    public:
        //! Called from the Evolution thread
        void draw( Int32 nCurIteration )
        {
            if (this->InvokeRequired)
            {
                drawDelegate^ d = gcnew drawDelegate(this, &Board::draw);
                IAsyncResult^ result = BeginInvoke(d, nCurIteration);
                EndInvoke(result);
                return;
            }
            m_counter->Text = nCurIteration.ToString();
            Invalidate();
        }
    
    public:
        Matrix *m_matrix;    

    private:
        SolidBrush^ m_occupiedBrush;
        SolidBrush^ m_freeBrush;
        Graphics^ m_graphics;
        Graphics^ m_mem_dc;
        Bitmap^ m_bmp;
        int m_width;
        int m_height;
        int m_squareSize;
        Label^ m_counter;
    };

