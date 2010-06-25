/* -*-c++-*- */
/* osgEarth - Dynamic map generation toolkit for OpenSceneGraph
 * Copyright 2008-2009 Pelican Ventures, Inc.
 * http://osgearth.org
 *
 * osgEarth is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#include <osgEarth/HeightFieldUtils>
#include <osgEarth/GeoData>
#include <osg/Notify>

using namespace osgEarth;

float
HeightFieldUtils::getHeightAtPixel(const osg::HeightField* hf, double c, double r, ElevationInterpolation interpolation)
{
    float result = 0.0;
    if (interpolation == INTERP_NEAREST)
    {
        //Nearest interpolation
        result = hf->getHeight((unsigned int)osg::round(c), (unsigned int)osg::round(r));
    }
    else if (interpolation == INTERP_TRIANGULATE)
    {
        //Interpolation to make sure that the interpolated point follows the triangles generated by the 4 parent points
        int rowMin = osg::maximum((int)floor(r), 0);
        int rowMax = osg::maximum(osg::minimum((int)ceil(r), (int)(hf->getNumRows()-1)), 0);
        int colMin = osg::maximum((int)floor(c), 0);
        int colMax = osg::maximum(osg::minimum((int)ceil(c), (int)(hf->getNumColumns()-1)), 0);

        if (rowMin == rowMax)
        {
            if (rowMin < hf->getNumRows()-2)
            {
                rowMax = rowMin + 1;
            }
            else
            {
                rowMin = rowMax - 1;
            }
         }

         if (colMin == colMax)
         {
            if (colMin < hf->getNumColumns()-2)
            {
                colMax = colMin + 1;
            }
            else
            {
               colMin = colMax - 1;
            }
         }

        if (rowMin > rowMax) rowMin = rowMax;
        if (colMin > colMax) colMin = colMax;

        float urHeight = hf->getHeight(colMax, rowMax);
        float llHeight = hf->getHeight(colMin, rowMin);
        float ulHeight = hf->getHeight(colMin, rowMax);
        float lrHeight = hf->getHeight(colMax, rowMin);

        //Make sure not to use NoData in the interpolation
        if (urHeight == NO_DATA_VALUE || llHeight == NO_DATA_VALUE || ulHeight == NO_DATA_VALUE || lrHeight == NO_DATA_VALUE)
        {
            return NO_DATA_VALUE;
        }

        double dx = c - (double)colMin;
        double dy = r - (double)rowMin;

        //The quad consisting of the 4 corner points can be made into two triangles.
        //The "left" triangle is ll, ur, ul
        //The "right" triangle is ll, lr, ur

        //Determine which triangle the point falls in.
        osg::Vec3d v0, v1, v2;
        if (dx > dy)
        {
            //The point lies in the right triangle
            v0.set(colMin, rowMin, llHeight);
            v1.set(colMax, rowMin, lrHeight);
            v2.set(colMax, rowMax, urHeight);
        }
        else
        {
            //The point lies in the left triangle
            v0.set(colMin, rowMin, llHeight);
            v1.set(colMax, rowMax, urHeight);
            v2.set(colMin, rowMax, ulHeight);
        }

        //Compute the normal
        osg::Vec3d n = (v1 - v0) ^ (v2 - v0);

        result = ( n.x() * ( c - v0.x() ) + n.y() * ( r - v0.y() ) ) / -n.z() + v0.z();
    }
    else
    {
        //OE_INFO << "getHeightAtPixel: (" << c << ", " << r << ")" << std::endl;
        int rowMin = osg::maximum((int)floor(r), 0);
        int rowMax = osg::maximum(osg::minimum((int)ceil(r), (int)(hf->getNumRows()-1)), 0);
        int colMin = osg::maximum((int)floor(c), 0);
        int colMax = osg::maximum(osg::minimum((int)ceil(c), (int)(hf->getNumColumns()-1)), 0);

        if (rowMin > rowMax) rowMin = rowMax;
        if (colMin > colMax) colMin = colMax;

        float urHeight = hf->getHeight(colMax, rowMax);
        float llHeight = hf->getHeight(colMin, rowMin);
        float ulHeight = hf->getHeight(colMin, rowMax);
        float lrHeight = hf->getHeight(colMax, rowMin);

        //Make sure not to use NoData in the interpolation
        if (urHeight == NO_DATA_VALUE || llHeight == NO_DATA_VALUE || ulHeight == NO_DATA_VALUE || lrHeight == NO_DATA_VALUE)
        {
            return NO_DATA_VALUE;
        }

        //OE_INFO << "Heights (ll, lr, ul, ur) ( " << llHeight << ", " << urHeight << ", " << ulHeight << ", " << urHeight << std::endl;

        if (interpolation == INTERP_BILINEAR)
        {
            //Check for exact value
            if ((colMax == colMin) && (rowMax == rowMin))
            {
                //OE_NOTICE << "Exact value" << std::endl;
                result = hf->getHeight((int)c, (int)r);
            }
            else if (colMax == colMin)
            {
                //OE_NOTICE << "Vertically" << std::endl;
                //Linear interpolate vertically
                result = ((double)rowMax - r) * llHeight + (r - (double)rowMin) * ulHeight;
            }
            else if (rowMax == rowMin)
            {
                //OE_NOTICE << "Horizontally" << std::endl;
                //Linear interpolate horizontally
                result = ((double)colMax - c) * llHeight + (c - (double)colMin) * lrHeight;
            }
            else
            {
                //OE_NOTICE << "Bilinear" << std::endl;
                //Bilinear interpolate
                float r1 = ((double)colMax - c) * llHeight + (c - (double)colMin) * lrHeight;
                float r2 = ((double)colMax - c) * ulHeight + (c - (double)colMin) * urHeight;

                //OE_INFO << "r1, r2 = " << r1 << " , " << r2 << std::endl;

                result = ((double)rowMax - r) * r1 + (r - (double)rowMin) * r2;
            }
        }
        else if (interpolation == INTERP_AVERAGE)
        {
            double x_rem = c - (int)c;
            double y_rem = r - (int)r;

            double w00 = (1.0 - y_rem) * (1.0 - x_rem) * (double)llHeight;
            double w01 = (1.0 - y_rem) * x_rem * (double)lrHeight;
            double w10 = y_rem * (1.0 - x_rem) * (double)ulHeight;
            double w11 = y_rem * x_rem * (double)urHeight;

            result = (float)(w00 + w01 + w10 + w11);
        }
    }

    return result;
}

float
HeightFieldUtils::getHeightAtLocation(const osg::HeightField* hf, double x, double y, double llx, double lly, double dx, double dy, ElevationInterpolation interpolation)
{
    //Determine the pixel to sample
    double px = osg::clampBetween( (x - llx) / dx, 0.0, (double)(hf->getNumColumns()-1) );
    double py = osg::clampBetween( (y - lly) / dy, 0.0, (double)(hf->getNumRows()-1) );
    return getHeightAtPixel(hf, px, py, interpolation);
}


void
HeightFieldUtils::scaleHeightFieldToDegrees( osg::HeightField* hf )
{
    if (hf)
    {
        //The number of degrees in a meter at the equator
        //TODO: adjust this calculation based on the actual EllipsoidModel.
        float scale = 1.0f/111319.0f;

        for (unsigned int i = 0; i < hf->getHeightList().size(); ++i)
        {
            hf->getHeightList()[i] *= scale;
        }
    }
    else
    {
        OE_WARN << "[osgEarth::HeightFieldUtils] scaleHeightFieldToDegrees heightfield is NULL" << std::endl;
    }
}


osg::HeightField*
HeightFieldUtils::createSubSample(osg::HeightField* input, const GeoExtent& inputEx, 
                                  const GeoExtent& outputEx, osgEarth::ElevationInterpolation interpolation)
{
    double div = outputEx.width()/inputEx.width();
    if ( div >= 1.0f )
        return 0L;

    int numCols = input->getNumColumns();
    int numRows = input->getNumRows();

    //float dx = input->getXInterval() * div;
    //float dy = input->getYInterval() * div;

    double xInterval = inputEx.width()  / (double)(input->getNumColumns()-1);
    double yInterval = inputEx.height()  / (double)(input->getNumRows()-1);
    double dx = div * xInterval;
    double dy = div * yInterval;


    osg::HeightField* dest = new osg::HeightField();
    dest->allocate( numCols, numRows );
    dest->setXInterval( dx );
    dest->setYInterval( dy );

    // copy over the skirt height, adjusting it for relative tile size.
    dest->setSkirtHeight( input->getSkirtHeight() * div );

    double x, y;
    int col, row;

    for( x = outputEx.xMin(), col=0; col < numCols; x += dx, col++ )
    {
        for( y = outputEx.yMin(), row=0; row < numRows; y += dy, row++ )
        {
            float height = HeightFieldUtils::getHeightAtLocation( input, x, y, inputEx.xMin(), inputEx.yMin(), xInterval, yInterval, interpolation);
            dest->setHeight( col, row, height );
        }
    }

    osg::Vec3d orig( outputEx.xMin(), outputEx.yMin(), input->getOrigin().z() );
    dest->setOrigin( orig );

    return dest;
}


/******************************************************************************************/

ReplaceInvalidDataOperator::ReplaceInvalidDataOperator():
_replaceWith(0.0f)
{
}

void
ReplaceInvalidDataOperator::operator ()(osg::HeightField *heightField)
{
    if (heightField && _validDataOperator.valid())
    {
        for (unsigned int i = 0; i < heightField->getHeightList().size(); ++i)
        {
            float elevation = heightField->getHeightList()[i];
            if (!(*_validDataOperator)(elevation))
            {
                heightField->getHeightList()[i] = _replaceWith;
            }
        }
    }
}


/******************************************************************************************/
FillNoDataOperator::FillNoDataOperator():
_defaultValue(0.0f)
{
}

void
FillNoDataOperator::operator ()(osg::HeightField *heightField)
{
    if (heightField && _validDataOperator.valid())
    {
        for( unsigned int row=0; row < heightField->getNumRows(); row++ )
        {
            for( unsigned int col=0; col < heightField->getNumColumns(); col++ )
            {
                float val = heightField->getHeight(col, row);

                if (!(*_validDataOperator)(val))
                {
                    if ( col > 0 )
                        val = heightField->getHeight(col-1,row);
                    else if ( col <= heightField->getNumColumns()-1 )
                        val = heightField->getHeight(col+1,row);

                    if (!(*_validDataOperator)(val))
                    {
                        if ( row > 0 )
                            val = heightField->getHeight(col, row-1);
                        else if ( row < heightField->getNumRows()-1 )
                            val = heightField->getHeight(col, row+1);
                    }

                    if (!(*_validDataOperator)(val))
                    {
                        val = _defaultValue;
                    }

                    heightField->setHeight( col, row, val );
                }
            }
        }
    }
}