#include "catch/catch.hpp"
#include "crafting.h"
#include "game.h"
#include "itype.h"
#include "map_helpers.h"
#include "npc.h"
#include "player.h"
#include "player_helpers.h"
#include "recipe_dictionary.h"
#include "recipe.h"
#include "requirements.h"
#include "test_statistics.h"
#include "recipe_dictionary.h"
#include "item.h"

struct all_stats {
    statistics<int> calories;
};

// given a list of components, adds all the calories together
int comp_calories( std::vector<item_comp> components )
{
    int calories = 0;
    for( item_comp it : components ) {
        auto temp = item::find_type( it.type )->comestible;
        if( temp && temp->cooks_like.empty() ) {
            calories += temp->get_calories() * it.count;
        } else if( temp ) {
            calories += item::find_type( temp->cooks_like )->comestible->get_calories() * it.count;
        }
    }
    return calories;
}

// puts one permutation of item components into a vector
std::vector<item_comp> item_comp_vector_create( const std::vector<std::vector<item_comp>> &vv,
        const std::vector<int> &ndx )
{
    std::vector<item_comp> list;
    for( int i = 0, sz = vv.size(); i < sz; ++i ) {
        list.emplace_back( vv[i][ndx[i]] );
    }
    return list;
}

std::vector<std::vector<item_comp>> recipe_permutations( const
                                 std::vector< std::vector< item_comp > > &vv )
{
    std::vector<int> muls;
    std::vector<int> szs;
    std::vector<std::vector<item_comp>> output;

    int total_mul = 1;
    int sz = vv.size();

    // Collect multipliers and sizes
    for( const std::vector<item_comp> &iv : vv ) {
        muls.push_back( total_mul );
        szs.push_back( iv.size() );
        total_mul *= iv.size();
    }

    // total_mul is number of [ v.pick(1) : vv] there are
    // iterate over each
    // container to hold the indices:
    std::vector<int> ndx;
    ndx.resize( sz );
    for( int i = 0; i < total_mul; ++i ) {
        for( int j = 0; j < sz; ++j ) {
            ndx[j] = ( i / muls[j] ) % szs[j];
        }

        // Do thing with indices
        output.emplace_back( item_comp_vector_create( vv, ndx ) );
    }
    return output;
}

int byproduct_calories( recipe recipe_obj )
{

    std::vector<item> byproducts = recipe_obj.create_byproducts();
    int kcal = 0;
    for( const item &it : byproducts ) {
        if( it.is_comestible() ) {
            kcal += it.type->comestible->get_calories();
        }
    }
    return kcal;
}

void print_itemcomp( std::vector<item_comp> list )
{
    printf( "error in permutation. list of components:\n" );
    for( const item_comp &itc : list ) {
        printf( "%s, %d\n", itc.to_string().c_str(), itc.count );
    }
    printf( "\n" );
}

all_stats run_stats( std::vector<std::vector<item_comp>> permutations, int byproduct_calories )
{
    all_stats mystats;
    for( const std::vector<item_comp> &permut : permutations ) {
        mystats.calories.add( comp_calories( permut ) - byproduct_calories );
    }
    return mystats;
}

item food_or_food_container( item it )
{
    return it.is_food_container() ? it.contents.front() : it;
}

TEST_CASE( "recipe_permutations" )
{
    for( auto i = recipe_dict.begin(); i != recipe_dict.end(); i++ ) {
        // the resulting item
        recipe recipe_obj = i->first.obj();
        item res_it = food_or_food_container( recipe_obj.create_result() );
        const bool is_food = res_it.is_food();
        const bool has_override = res_it.has_flag( "NUTRIENT_OVERRIDE" );
        if( is_food && !has_override ) {
            all_stats mystats = run_stats( recipe_permutations( recipe_obj.requirements().get_components() ),
                                           byproduct_calories( recipe_obj ) );
            int default_calories = 0;
            if( res_it.type->comestible ) {
                default_calories = res_it.type->comestible->get_calories();
            }
            if( res_it.charges > 0 ) {
                default_calories *= res_it.charges;
            }
            const float lower_bound = std::min( default_calories - mystats.calories.stddev() * 2,
                                                default_calories * 0.8 );
            const float upper_bound = std::max( default_calories + mystats.calories.stddev() * 2,
                                                default_calories * 1.2 );
            if( mystats.calories.min() != mystats.calories.max() ) {
                if( mystats.calories.min() < 0 || lower_bound >= mystats.calories.avg() ||
                    mystats.calories.avg() >= upper_bound ) {
                    printf( "\n\nRecipeID: %s, Lower Bound: %f, Average: %f, Upper Bound: %f\n\n", i->first.c_str(),
                            lower_bound, mystats.calories.avg(), upper_bound );
                }
                CHECK( mystats.calories.min() >= 0 );
                REQUIRE( lower_bound < mystats.calories.avg() );
                REQUIRE( mystats.calories.avg() < upper_bound );
            }
        }
    }
}
