#!/bin/sh

make run_fresh

dot -Tpng ast.dot -o ast.png
dot -Tpng ir_plan.dot -o ir_plan.png

dot -Tpng ir_plan_semi_j.dot -o ir_plan_semi_j.png
dot -Tpng ir_plan_agg_opt.dot -o ir_plan_agg_opt.png
dot -Tpng ir_plan_mat.dot -o ir_plan_mat.png
dot -Tpng ir_plan_mat_num.dot -o ir_plan_mat_num.png
dot -Tpng api_plan.dot -o api_plan.png

dot -Tpng ir_plan_semi_j_l.dot -o ir_plan_semi_j_l.png
dot -Tpng ir_plan_agg_opt_l.dot -o ir_plan_agg_opt_l.png
dot -Tpng ir_plan_mat_l_gco.dot -o ir_plan_mat_l_gco.png
dot -Tpng ir_plan_mat_num_l.dot -o ir_plan_mat_num_l.png
dot -Tpng api_plan_l.dot -o api_plan_l.png

dot -Tpng ir_plan_semi_j_l2.dot -o ir_plan_semi_j_l2.png
dot -Tpng ir_plan_agg_opt_l2.dot -o ir_plan_agg_opt_l2.png
dot -Tpng ir_plan_mat_l2.dot -o ir_plan_mat_l2.png
dot -Tpng ir_plan_mat_l2_gco.dot -o ir_plan_mat_l2_gco.png
dot -Tpng ir_plan_mat_num_l2.dot -o ir_plan_mat_num_l2.png
dot -Tpng api_plan_l2.dot -o api_plan_l2.png

dot -Tpng ir_plan_semi_j_lh.dot -o ir_plan_semi_j_lh.png
dot -Tpng ir_plan_agg_opt_lh.dot -o ir_plan_agg_opt_lh.png
dot -Tpng ir_plan_mat_lh.dot -o ir_plan_mat_lh.png
dot -Tpng ir_plan_mat_lh_gco.dot -o ir_plan_mat_lh_gco.png
dot -Tpng ir_plan_mat_num_lh.dot -o ir_plan_mat_num_lh.png
dot -Tpng api_plan_lh.dot -o api_plan_lh.png